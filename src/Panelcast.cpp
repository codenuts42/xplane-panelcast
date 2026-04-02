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

static GLint g_tex = 0;
static int g_width = 0;
static int g_height = 0;

static GLuint g_pbo[2] = {0, 0};
static int g_pboIndex = 0;
static bool g_pboInitialized = false;

static std::mutex g_frameMutex;
static std::queue<std::vector<unsigned char>> g_frameQueue;
static uint32_t g_frameCounter = 0;

static std::atomic<bool> g_running{false};
static std::thread g_networkThread;

static int g_sock = -1;
static sockaddr_in g_destAddr;

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
	if (g_pboInitialized || width <= 0 || height <= 0)
		return;

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

	if (compressedSize <= 0)
		return;

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

		if (!frame.empty()) {
			sendFrame(frame);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

// ------------------------------------------------------------
// Panel capturen
// ------------------------------------------------------------

// Panel‑Texturgröße automatisch ermitteln
static void updatePanelSizeFromTexture(GLuint tex) {
	if (tex == 0)
		return;

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

struct Point {
	int x, y;
};

struct Color {
	float r, g, b, a;
};

void drawPolygon(int x, int y, int width, int height) {
	glBegin(GL_POLYGON);
	glVertex2i(x, y);
	glVertex2i(x, y + height);
	glVertex2i(x + width, y + height);
	glVertex2i(x + width, y);
	glEnd();
}

static int findTexture() {
	// draw markerpoint
	Point markerpoint = {100, 100};
	Color markercolor = {1.0f, 0.0f, 0.0f, 1.0f}; // red
	XPLMSetGraphicsState(0, 0, 0, 0, 0, 0, 0);
	glColor4f(markercolor.r, markercolor.g, markercolor.b, markercolor.a);
	drawPolygon(markerpoint.x, markerpoint.y, 1, 1);

	// find texture
	int width, height;
	for (int i = 0; i < 100; i++) {
		XPLMBindTexture2d(i, 0);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
		if (width > 0 && height > 0) {
			logger.log("Texture found id=%d (%d x %d)", i, width, height);
			std::vector<unsigned char> pixels(width * height * 4);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			int idx = (markerpoint.y * width + markerpoint.x) * 4;
			if (pixels[idx] == markercolor.r && pixels[idx + 1] == markercolor.g && pixels[idx + 2] == markercolor.b &&
			    pixels[idx + 3] == markercolor.a) {
				return i;
			}
		}
	}
	return 0;
}

static int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon) {

	if (g_tex == 0) {
		g_tex = findTexture();
	}

	if (g_tex == 0) {
		return 1;
	}

	if (g_width == 0 || g_height == 0) {
		updatePanelSizeFromTexture(g_tex);
	}

	if (g_width == 0 || g_height == 0) {
		return 1;
	}

	if (!g_pboInitialized) {
		initPBOs(g_width, g_height);
	}

	if (g_tex != 0) {
		capturePanel(g_tex);
	}

	return 1;
}

// -----------------------------
// Plugin‑Entry‑Points
// -----------------------------

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
	std::strcpy(outName, "Panelcast");
	std::strcpy(outSig, "de.codenuts.panelcast");
	std::strcpy(outDesc, "Streaming von 2D‑Cockpit‑Panels über Netzwerk.");

	initUDP("127.0.0.1", 5000); // IP/Port anpassen

#ifdef _WIN32
	gladLoadGL();
#endif

	XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);

	g_running.store(true);
	g_networkThread = std::thread(networkThreadLoop);

	return 1;
}

PLUGIN_API void XPluginStop(void) {
	XPLMRegisterDrawCallback(drawCallback, xplm_Phase_Gauges, 0, nullptr);

	g_running.store(false);
	if (g_networkThread.joinable()) {
		g_networkThread.join();
	}
	closeUDP();
}

PLUGIN_API int XPluginEnable(void) {
	return 1;
}

PLUGIN_API void XPluginDisable(void) {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {}
