// =====================================================================
// Panelcast.cpp – finale Version mit dynamischem ROI per DataRefs
// Liest direkt den Panel‑Framebuffer (xplm_Phase_Gauges) und streamt
// einen per DataRefs einstellbaren Ausschnitt über UDP (LZ4).
// =====================================================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <glad/glad.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Logger.h"
#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlugin.h"
#include "lz4/lz4.h"

Logger logger("[Panelcast]");

// =====================================================================
// ROI Panels
// =====================================================================
struct PanelROI {
	uint16_t panelID;
	int x, y;
	int w, h;
};

static std::vector<PanelROI> g_panels = {
    {0, 5, 1543, 500, 500},  // PFD
    {1, 515, 1543, 500, 500} //, // ND
    //{2, 100, 950, 600, 400}, // EICAS
};

// =====================================================================
// PBO
// =====================================================================
static GLuint g_pbo[2] = {0, 0};
static int g_pboIndex = 0;
static bool g_pboInitialized = false;
static int g_pboWidth = 0;
static int g_pboHeight = 0;

// =====================================================================
// Netzwerk
// =====================================================================
static int g_sock = -1;
static sockaddr_in g_destAddr;

struct RawPanelFrame {
	uint16_t panelID;
	int width;
	int height;
	std::vector<char> pixels;
};

static std::mutex g_rawMutex;
static std::unordered_map<uint16_t, RawPanelFrame> g_latestFrames;

static std::atomic<bool> g_running{false};
static std::thread g_workerThread;

static uint32_t g_frameCounter = 0;

// =====================================================================
// UDP
// =====================================================================

static void initUDP(const char* ip, uint16_t port) {
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	g_sock = (int)socket(AF_INET, SOCK_DGRAM, 0);

	memset(&g_destAddr, 0, sizeof(g_destAddr));
	g_destAddr.sin_family = AF_INET;
	g_destAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &g_destAddr.sin_addr);
}

static void closeUDP() {
	if (g_sock >= 0) {
#ifdef _WIN32
		closesocket(g_sock);
		WSACleanup();
#else
		close(g_sock);
#endif
		g_sock = -1;
	}
}

// =====================================================================
// PBO initialisieren / neu anpassen
// =====================================================================
struct PanelCaptureState {
	GLuint pbo[2] = {0, 0};
	int pboIndex = 0;
	bool initialized = false;
	int w = 0;
	int h = 0;
};

static std::unordered_map<uint16_t, PanelCaptureState> g_panelState;

static void initOrResizePanelPBOs(uint16_t panelID, int w, int h) {
	auto& st = g_panelState[panelID];

	if (st.initialized && st.w == w && st.h == h)
		return;

	// Alte PBOs löschen
	if (st.initialized) {
		glDeleteBuffers(2, st.pbo);
	}

	glGenBuffers(2, st.pbo);

	for (int i = 0; i < 2; i++) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, st.pbo[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, w * h * 4, nullptr, GL_STREAM_READ);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	st.pboIndex = 0;
	st.initialized = true;
	st.w = w;
	st.h = h;
}

// =====================================================================
// Capture
// =====================================================================

static void captureSinglePanel(const PanelROI& roi) {
	int w = roi.w;
	int h = roi.h;

	auto& st = g_panelState[roi.panelID];

	initOrResizePanelPBOs(roi.panelID, w, h);
	if (!st.initialized)
		return;

	int next = (st.pboIndex + 1) % 2;

	// 1. Frame in PBO schreiben
	glBindBuffer(GL_PIXEL_PACK_BUFFER, st.pbo[st.pboIndex]);
	glReadPixels(roi.x, roi.y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// 2. Vorherigen PBO auslesen
	glBindBuffer(GL_PIXEL_PACK_BUFFER, st.pbo[next]);
	unsigned char* ptr = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

	if (ptr) {
		RawPanelFrame f;
		f.panelID = roi.panelID;
		f.width = w;
		f.height = h;
		f.pixels.resize(w * h * 4);
		memcpy(f.pixels.data(), ptr, w * h * 4);

		{
			std::lock_guard<std::mutex> lock(g_rawMutex);
			g_latestFrames[roi.panelID] = std::move(f);
		}

		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	st.pboIndex = next;
}

static void captureAllPanels() {
	for (const auto& p : g_panels) { captureSinglePanel(p); }
}

// =====================================================================
// Frame senden
// =====================================================================

struct PanelFragmentHeader {
	uint32_t magic;
	uint32_t frameID;
	uint16_t panelID;
	uint16_t fragIndex;
	uint16_t fragCount;
	uint16_t panelCount; // optional
	uint32_t payloadSize;
	uint16_t width;
	uint16_t height;
	uint32_t compressedSize;
};

static void sendPanelFragments(uint16_t panelID, uint32_t frameID, const char* compData, int compSize, int w, int h) {
	const int headerSize = sizeof(PanelFragmentHeader);
	const int maxPayload = 1472 - headerSize; // optimale MTU

	int fragCount = (compSize + maxPayload - 1) / maxPayload;

	PanelFragmentHeader hdr;
	hdr.magic = 0xABCD1234;
	hdr.frameID = frameID;
	hdr.panelID = panelID;
	hdr.fragCount = fragCount;
	hdr.width = w;
	hdr.height = h;
	hdr.compressedSize = compSize;

	for (int i = 0; i < fragCount; i++) {
		int offset = i * maxPayload;
		int chunkSize = std::min(maxPayload, compSize - offset);

		hdr.fragIndex = i;
		hdr.payloadSize = chunkSize;

		WSABUF bufs[2];
		bufs[0].buf = (CHAR*)&hdr;
		bufs[0].len = headerSize;
		bufs[1].buf = (CHAR*)(compData + offset);
		bufs[1].len = chunkSize;

		DWORD sent = 0;
		WSASendTo(g_sock, bufs, 2, &sent, 0, (sockaddr*)&g_destAddr, sizeof(g_destAddr), NULL, NULL);
	}
}

static void compressAndSendPanel(const RawPanelFrame& f) {
	int rawSize = f.width * f.height * 4;

	int maxComp = LZ4_compressBound(rawSize);
	std::vector<char> comp(maxComp);

	int compSize = LZ4_compress_fast(f.pixels.data(), comp.data(), rawSize, maxComp, 8);

	if (compSize <= 0)
		return;

	comp.resize(compSize);

	uint32_t frameID = g_frameCounter++;

	sendPanelFragments(f.panelID, frameID, comp.data(), compSize, f.width, f.height);
}

static void workerThreadLoop() {
	while (g_running.load()) {

		std::unordered_map<uint16_t, RawPanelFrame> frames;

		{
			std::lock_guard<std::mutex> lock(g_rawMutex);
			frames = g_latestFrames;
			g_latestFrames.clear();
		}

		for (auto& [panelID, frame] : frames) { compressAndSendPanel(frame); }

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

// =====================================================================
// Draw‑Callback
// =====================================================================

static int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon) {
	// "Letzten" FBO ermitteln und nur dann Capturen
	static GLint maxFBO = 0;
	GLint currentFBO = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
	if (currentFBO < maxFBO)
		return 1;
	maxFBO = currentFBO;

	captureAllPanels();
	return 1;
}

// =====================================================================
// Plugin‑Lifecycle
// =====================================================================

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
	std::strcpy(outName, "Panelcast");
	std::strcpy(outSig, "de.codenuts.panelcast");
	std::strcpy(outDesc, "Streaming eines Panel‑Ausschnitts über Netzwerk.");

	initUDP("127.0.0.1", 5000);

#ifdef _WIN32
	gladLoadGL();
#endif

	XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);

	g_running.store(true);
	g_workerThread = std::thread(workerThreadLoop);

	return 1;
}

PLUGIN_API void XPluginStop(void) {
	XPLMUnregisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);

	g_running.store(false);
	if (g_workerThread.joinable())
		g_workerThread.join();

	closeUDP();

	if (g_pboInitialized) {
		glDeleteBuffers(2, g_pbo);
		g_pboInitialized = false;
	}
}

PLUGIN_API int XPluginEnable(void) {
	return 1;
}
PLUGIN_API void XPluginDisable(void) {}
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {}
