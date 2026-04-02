// =====================================================================
//
// Panelcast.cpp
//
// X-Plane 12 Plugin zum Streaming von 2D‑Cockpit‑Panels über Netzwerk
//
// =====================================================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <glad/glad.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include <GL/gl.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#endif

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Logger.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlugin.h"
#include "lz4/lz4.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

// -----------------------------
// Globale Variablen
// -----------------------------

Logger logger("[Panelcast]");

static int g_width = 0;
static int g_height = 0;

static GLuint g_pbo[2] = {0, 0};
static int g_pboIndex = 0;
static bool g_pboInitialized = false;

static std::mutex g_frameMutex;
static std::queue<std::vector<unsigned char>> g_frameQueue;

static std::atomic<bool> g_running{false};
static std::thread g_networkThread;

static int g_sock = -1;
static sockaddr_in g_destAddr;

static uint32_t g_frameCounter = 0;

static GLint g_tex = 0;

// Functions
int handle_command(XPLMCommandRef cmd_id, XPLMCommandPhase phase, void* in_refcon);
XPLMCommandRef cmd_texid_inc = NULL;
XPLMCommandRef cmd_texid_dec = NULL;

static int beforePanelCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon);
static int afterPanelCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon);
void registerDrawCallback();
void unregisterDrawCallback();
void registerCallbacks();
void unregisterCallbacks();

// -----------------------------
// UDP initialisieren / Cleanup
// -----------------------------

static void initUDP(const char* ip, uint16_t port) {
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	g_sock = (int)socket(AF_INET, SOCK_DGRAM, 0);

	memset(&g_destAddr, 0, sizeof(g_destAddr));
	g_destAddr.sin_family = AF_INET;
	g_destAddr.sin_port = htons(port);
	g_destAddr.sin_family = AF_INET;
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

// -----------------------------
// PBO initialisieren
// -----------------------------

static void initPBOs(int width, int height) {
	if (g_pboInitialized || width <= 0 || height <= 0) return;

	glGenBuffers(2, g_pbo);
	for (int i = 0; i < 2; ++i) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, width * height * 4, nullptr, GL_STREAM_READ);
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	g_pboInitialized = true;
}

// ------------------------------------------------------------
// Frame in Queue legen
// ------------------------------------------------------------

static void enqueueFrame(const unsigned char* ptr, int width, int height) {
	const size_t size = static_cast<size_t>(width) * height * 4;
	std::vector<unsigned char> frame(size);
	std::memcpy(frame.data(), ptr, size);

	std::lock_guard<std::mutex> lock(g_frameMutex);
	g_frameQueue.push(std::move(frame));

	// Queue begrenzen, damit wir nicht hinterherhinken
	while (g_frameQueue.size() > 3) { g_frameQueue.pop(); }
}

// ------------------------------------------------------------
// LZ4 + UDP‑Streaming
// ------------------------------------------------------------

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

static void sendFrame(const std::vector<unsigned char>& frame) {
	const int width = g_width;
	const int height = g_height;
	const int rawSize = width * height * 4;

	// LZ4 komprimieren
	int maxCompressed = LZ4_compressBound(rawSize);
	std::vector<char> compressed(maxCompressed);

	int compressedSize = LZ4_compress_default((const char*)frame.data(), compressed.data(), rawSize, maxCompressed);

	if (compressedSize <= 0) return;

	compressed.resize(compressedSize);

	// Fragmentierung
	const int MTU = 1300; // sicher unter 1500
	const int headerSize = sizeof(FrameHeader);
	const int maxPayload = MTU - headerSize;

	int fragCount = (compressedSize + maxPayload - 1) / maxPayload;

	uint32_t frameID = g_frameCounter++;

	for (int i = 0; i < fragCount; i++) {
		int offset = i * maxPayload;
		int chunkSize = fmin(maxPayload, compressedSize - offset);

		FrameHeader hdr;
		hdr.magic = 0xABCD1234;
		hdr.frameID = frameID;
		hdr.fragIndex = i;
		hdr.fragCount = fragCount;
		hdr.payloadSize = chunkSize;
		hdr.width = width;
		hdr.height = height;
		hdr.rawSize = rawSize;
		hdr.compressedSize = compressedSize;

		// Paket bauen
		std::vector<char> packet;
		packet.resize(headerSize + chunkSize);

		memcpy(packet.data(), &hdr, headerSize);
		memcpy(packet.data() + headerSize, compressed.data() + offset, chunkSize);

		// UDP senden
		sendto(g_sock, packet.data(), packet.size(), 0, (sockaddr*)&g_destAddr, sizeof(g_destAddr));
	}
}

// ------------------------------------------------------------
// Netzwerk‑Thread
// ------------------------------------------------------------

static void networkThreadLoop() {
	while (g_running.load()) {
		std::vector<unsigned char> frame;

		{
			std::lock_guard<std::mutex> lock(g_frameMutex);
			if (!g_frameQueue.empty()) {
				frame = std::move(g_frameQueue.front());
				g_frameQueue.pop();
			}
		}

		if (!frame.empty()) { sendFrame(frame); }

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

// ------------------------------------------------------------
// Panel capturen
// ------------------------------------------------------------

static GLuint getCurrentBoundTexture2D() {
	return static_cast<GLuint>(g_tex);
}

// Panel‑Texturgröße automatisch ermitteln
static void updatePanelSizeFromTexture(GLuint tex) {
	if (tex == 0) return;

	glBindTexture(GL_TEXTURE_2D, tex);

	GLint w = 0, h = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

	if (w > 0 && h > 0) {
		g_width = w;
		g_height = h;
	}
}

// Panel capturen: GPU → PBO → Queue
static void capturePanel(GLuint texID) {
	if (texID == 0) return;

	if (g_width == 0 || g_height == 0) { updatePanelSizeFromTexture(texID); }
	if (g_width == 0 || g_height == 0) return;

	if (!g_pboInitialized) initPBOs(g_width, g_height);

	int next = (g_pboIndex + 1) % 2;

	// GPU → PBO (asynchron)
	glBindTexture(GL_TEXTURE_2D, texID);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[g_pboIndex]);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// CPU liest vorherigen PBO
	glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[next]);
	unsigned char* ptr = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

	if (ptr) {
		enqueueFrame(ptr, g_width, g_height);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	g_pboIndex = next;
}

// -----------------------------
// Draw‑Callback
// -----------------------------

static int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon) {
	// In dieser Phase ist die Panel‑Textur gebunden
	GLuint panelTex = getCurrentBoundTexture2D();
	if (panelTex != 0) { capturePanel(panelTex); }

	return 1;
}

// -----------------------------

void drawPolygon(int x, int y, int width, int height) {
	glBegin(GL_POLYGON);
	glVertex2i(x, y);
	glVertex2i(x, y + height);
	glVertex2i(x + width, y + height);
	glVertex2i(x + width, y);
	glEnd();
}

static int beforePanelCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon) {
	// State setzen
	XPLMSetGraphicsState(0, 0, 0, 0, 0, 0, 0);

	glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // red
	drawPolygon(100, 100, 1, 1);

	// Zeichnen
	// glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // red
	// drawPolygon(100, 0, 100, 1);
	//
	// glColor4f(0.0f, 1.0f, 0.0f, 1.0f); // green
	// drawPolygon(100, 1, 100, 1);
	//
	// glColor4f(0.0f, 0.0f, 1.0f, 1.0f); // blue
	// drawPolygon(100, 2, 100, 1);
	//
	// glColor4f(0.5f, 0.5f, 0.5f, 1.0f); // grey
	// drawPolygon(100, 3, 100, 1);

	// glColor4f(0.0f, 0.0f, 0.0f, 0.0f);

	return 1;
}

void logPixel(unsigned char* tex, int width, int x, int y) {
	int idx = (y * width + x) * 4;

	logger.log("Pixel (%d,%d) = R=%02x G=%02x B=%02x A=%02x\n", x, y, tex[idx + 0], tex[idx + 1], tex[idx + 2],
	           tex[idx + 3]);
}

static int afterPanelCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon) {
	static int countcalls = 0;
	countcalls++;

	int cockpit_texture_width = 4096;
	int cockpit_texture_height = 4096;
	int cockpit_texture_format = 32856;
	int tw, th, tf;
	unsigned char* texture_temp = (unsigned char*)malloc(cockpit_texture_width * cockpit_texture_height * 4);

	for (int i = 0; i < 100; i++) {
		XPLMBindTexture2d(i, 0);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &tf);

		if (tw > 0 && th > 0) {
			logger.log("Texture found id=%d (%d x %d)", i, tw, th);
			if (countcalls <= 2) {
				glBindTexture(GL_TEXTURE_2D, i);
				std::vector<unsigned char> pixels(tw * th * 4);
				glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				std::filesystem::create_directories("panelcast/" + std::to_string(countcalls));
				std::string filename =
				    "panelcast/" + std::to_string(countcalls) + "/Texture_" + std::to_string(i) + ".png";
				stbi_write_png(filename.c_str(), tw, th, 4, pixels.data(), tw * 4);
			}
		}

		if ((tw == cockpit_texture_width) && (th == cockpit_texture_height) && (tf == cockpit_texture_format)) {
			// Do expensive texture read-back since the dimensions are the same
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_temp);
			logger.log("Found candidate texture id=%d, width=%d, height=%d, internal "
			           "format == %d\n",
			           i, tw, th, tf);
			logPixel(texture_temp, cockpit_texture_width, 0, 0);
			logPixel(texture_temp, cockpit_texture_width, 0, 1);
			logPixel(texture_temp, cockpit_texture_width, 0, 2);
			logPixel(texture_temp, cockpit_texture_width, 0, 3);
			logPixel(texture_temp, cockpit_texture_width, 0, 4);
			logPixel(texture_temp, cockpit_texture_width, 10, 10);

			int x = 100;
			int y = 100;
			int idx = (y * cockpit_texture_width + x) * 4;
			if ((texture_temp[idx] == 0x00) && (texture_temp[idx + 1] == 0xFF) && (texture_temp[idx + 2] == 0x00) &&
			    (texture_temp[idx + 3] == 0xFF)) {
				logger.log("Texture id %d is a match from detected color\n", i);
				g_tex = i;
				unregisterCallbacks();
				registerDrawCallback();
				break; // Schleife sofort verlassen
			}
		}
	}

	return 1;
}

void registerDrawCallback() {
	XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);
}

void unregisterDrawCallback() {
	XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);
}

void registerCallbacks() {
	XPLMRegisterDrawCallback(beforePanelCallback, xplm_Phase_Panel, 1, nullptr);
	XPLMRegisterDrawCallback(afterPanelCallback, xplm_Phase_Panel, 0, nullptr);
}

void unregisterCallbacks() {
	XPLMUnregisterDrawCallback(beforePanelCallback, xplm_Phase_Panel, 1, nullptr);
	XPLMUnregisterDrawCallback(afterPanelCallback, xplm_Phase_Panel, 0, nullptr);
}

// -----------------------------
// Plugin‑Entry‑Points
// -----------------------------

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
	std::strcpy(outName, "Panelcast");
	std::strcpy(outSig, "de.codenuts.panelcast");
	std::strcpy(outDesc, "Streaming von 2D‑Cockpit‑Panels über Netzwerk.");

	cmd_texid_inc = XPLMCreateCommand("Panelcast/TEXTID_INC", "Panelcast Increase TexID");
	cmd_texid_dec = XPLMCreateCommand("Panelcast/TEXTID_DEC", "Panelcast Decrease TexID");
	XPLMRegisterCommandHandler(cmd_texid_inc, handle_command, 1, (void*)"Increase TexID");
	XPLMRegisterCommandHandler(cmd_texid_dec, handle_command, 1, (void*)"Decrease TexID");

	initUDP("127.0.0.1", 5000); // IP/Port anpassen

#ifdef _WIN32
	gladLoadGL();
#endif
	registerCallbacks();

	g_running.store(true);
	g_networkThread = std::thread(networkThreadLoop);

	return 1;
}

PLUGIN_API void XPluginStop(void) {
	unregisterCallbacks();
	unregisterDrawCallback();

	g_running.store(false);
	if (g_networkThread.joinable()) g_networkThread.join();

	closeUDP();
}

PLUGIN_API int XPluginEnable(void) {
	return 1;
}

PLUGIN_API void XPluginDisable(void) {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {}

int handle_command(XPLMCommandRef cmd_id, XPLMCommandPhase phase, void* in_refcon) {
	// Only do the command when it is being released
	if (phase == xplm_CommandEnd) {
		logger.log("Incoming command %p with reference [%s]\n", cmd_id, (char*)in_refcon);
		if (cmd_id == cmd_texid_inc) {
			g_tex++;
			g_width = 0;
			g_pboInitialized = false;
			logger.log("TexID Increased: %d", g_tex);
		}
		if (cmd_id == cmd_texid_dec) {
			if (g_tex > 0) {
				g_tex--;
				g_width = 0;
				g_pboInitialized = false;
			}
			logger.log("TexID Decreased: %d", g_tex);
		}
	}
	return 1;
}
