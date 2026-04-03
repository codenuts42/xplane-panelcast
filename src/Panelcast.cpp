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
#include <vector>

#include "Logger.h"
#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlugin.h"
#include "lz4/lz4.h"

Logger logger("[Panelcast]");

// =====================================================================
// ROI‑Variablen (werden per DataRef gesetzt)
// =====================================================================
static int roi_x = 5;
static int roi_y = 1543;
static int roi_w = 1015;
static int roi_h = 500;

// =====================================================================
// DataRefs
// =====================================================================
static XPLMDataRef dr_roi_x = nullptr;
static XPLMDataRef dr_roi_y = nullptr;
static XPLMDataRef dr_roi_w = nullptr;
static XPLMDataRef dr_roi_h = nullptr;

// Getter/Setter
int get_roi_x(void*) {
	return roi_x;
}
void set_roi_x(void*, int v) {
	roi_x = v;
}

int get_roi_y(void*) {
	return roi_y;
}
void set_roi_y(void*, int v) {
	roi_y = v;
}

int get_roi_w(void*) {
	return roi_w;
}
void set_roi_w(void*, int v) {
	roi_w = v;
}

int get_roi_h(void*) {
	return roi_h;
}
void set_roi_h(void*, int v) {
	roi_h = v;
}

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

static std::mutex g_frameMutex;
static std::queue<std::vector<char>> g_frameQueue;
static std::atomic<bool> g_running{false};
static std::thread g_networkThread;
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

static void initOrResizePBOs(int w, int h) {
	if (w <= 0 || h <= 0)
		return;

	if (g_pboInitialized && w == g_pboWidth && h == g_pboHeight)
		return;

	if (g_pboInitialized) {
		glDeleteBuffers(2, g_pbo);
		g_pboInitialized = false;
	}

	const size_t size = (size_t)w * h * 4;

	glGenBuffers(2, g_pbo);
	for (int i = 0; i < 2; i++) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STREAM_READ);
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	g_pboInitialized = true;
	g_pboWidth = w;
	g_pboHeight = h;

	logger.log("PBOs initialized/resized to %dx%d", w, h);
}

// =====================================================================
// Kompression + Queue
// =====================================================================

static void compressAndEnqueue(const unsigned char* ptr, int w, int h) {
	if (!ptr || w <= 0 || h <= 0)
		return;

	const int rawSize = w * h * 4;
	int maxCompressed = LZ4_compressBound(rawSize);
	if (maxCompressed <= 0)
		return;

	std::vector<char> compressed(maxCompressed);

	int compressedSize = LZ4_compress_default((const char*)ptr, compressed.data(), rawSize, maxCompressed);

	if (compressedSize <= 0)
		return;

	compressed.resize(compressedSize);

	std::lock_guard<std::mutex> lock(g_frameMutex);
	g_frameQueue.push(std::move(compressed));

	while (g_frameQueue.size() > 3) g_frameQueue.pop();
}

// =====================================================================
// Frame senden
// =====================================================================

struct FrameHeader {
	uint32_t magic;
	uint32_t frameID;
	uint16_t fragIndex;
	uint16_t fragCount;
	uint32_t payloadSize;
	uint32_t width;
	uint32_t height;
	uint32_t rawSize;
	uint32_t compressedSize;
};

static void sendFrame(const std::vector<char>& data, int w, int h) {
	const int compressedSize = (int)data.size();
	if (compressedSize <= 0 || w <= 0 || h <= 0)
		return;

	const int rawSize = w * h * 4;

	const int MTU = 1300;
	const int headerSize = sizeof(FrameHeader);
	const int maxPayload = MTU - headerSize;

	int fragCount = (compressedSize + maxPayload - 1) / maxPayload;
	uint32_t frameID = g_frameCounter++;

	for (int i = 0; i < fragCount; i++) {
		int offset = i * maxPayload;
		if (offset >= compressedSize)
			break;

		int chunkSize = std::min(maxPayload, compressedSize - offset);

		FrameHeader hdr;
		hdr.magic = 0xABCD1234;
		hdr.frameID = frameID;
		hdr.fragIndex = (uint16_t)i;
		hdr.fragCount = (uint16_t)fragCount;
		hdr.payloadSize = (uint32_t)chunkSize;
		hdr.width = (uint32_t)w;
		hdr.height = (uint32_t)h;
		hdr.rawSize = (uint32_t)rawSize;
		hdr.compressedSize = (uint32_t)compressedSize;

		std::vector<char> packet(headerSize + chunkSize);
		memcpy(packet.data(), &hdr, headerSize);
		memcpy(packet.data() + headerSize, data.data() + offset, chunkSize);

		sendto(g_sock, packet.data(), (int)packet.size(), 0, (sockaddr*)&g_destAddr, sizeof(g_destAddr));
	}
}

// =====================================================================
// Netzwerk‑Thread
// =====================================================================

static void networkThreadLoop() {
	while (g_running.load()) {
		std::vector<char> frame;
		int w = 0, h = 0;

		{
			std::lock_guard<std::mutex> lock(g_frameMutex);
			if (!g_frameQueue.empty()) {
				frame = std::move(g_frameQueue.front());
				g_frameQueue.pop();
				w = g_pboWidth;
				h = g_pboHeight;
			}
		}

		if (!frame.empty() && w > 0 && h > 0)
			sendFrame(frame, w, h);

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

// =====================================================================
// Panel‑Framebuffer lesen
// =====================================================================

static void capturePanelRegion() {

	int x = roi_x;
	int y = roi_y;
	int w = roi_w;
	int h = roi_h;

	if (w <= 0 || h <= 0)
		return;

	initOrResizePBOs(w, h);
	if (!g_pboInitialized)
		return;

	int next = (g_pboIndex + 1) % 2;

	glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[g_pboIndex]);
	glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[next]);
	unsigned char* ptr = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

	if (ptr) {
		compressAndEnqueue(ptr, w, h);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	g_pboIndex = next;
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

	capturePanelRegion();
	return 1;
}

// =====================================================================
// Plugin‑Lifecycle
// =====================================================================

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
	std::strcpy(outName, "Panelcast");
	std::strcpy(outSig, "de.codenuts.panelcast");
	std::strcpy(outDesc, "Streaming eines Panel‑Ausschnitts über Netzwerk.");

	// DataRefs registrieren
	dr_roi_x =
	    XPLMRegisterDataAccessor("panelcast/roi_x", xplmType_Int, 1, get_roi_x, set_roi_x, nullptr, nullptr, nullptr,
	                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

	dr_roi_y =
	    XPLMRegisterDataAccessor("panelcast/roi_y", xplmType_Int, 1, get_roi_y, set_roi_y, nullptr, nullptr, nullptr,
	                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

	dr_roi_w =
	    XPLMRegisterDataAccessor("panelcast/roi_w", xplmType_Int, 1, get_roi_w, set_roi_w, nullptr, nullptr, nullptr,
	                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

	dr_roi_h =
	    XPLMRegisterDataAccessor("panelcast/roi_h", xplmType_Int, 1, get_roi_h, set_roi_h, nullptr, nullptr, nullptr,
	                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

	initUDP("127.0.0.1", 5000);

#ifdef _WIN32
	gladLoadGL();
#endif

	XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);

	g_running.store(true);
	g_networkThread = std::thread(networkThreadLoop);

	return 1;
}

PLUGIN_API void XPluginStop(void) {
	XPLMUnregisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);

	g_running.store(false);
	if (g_networkThread.joinable())
		g_networkThread.join();

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
