#include "Logger.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN

    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    #include <glad/glad.h>
    #include <GL/gl.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#endif

#include "Logger.h"
#include "XPLMPlugin.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>
#include <cstring>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "lz4/lz4.h"

// -----------------------------
// Globale Variablen
// -----------------------------

Logger logger("[Panelcast]");

static int g_width  = 0;
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

// -----------------------------
// UDP initialisieren / Cleanup
// -----------------------------

static void initUDP(const char* ip, uint16_t port)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    g_sock = (int)socket(AF_INET, SOCK_DGRAM, 0);

    memset(&g_destAddr, 0, sizeof(g_destAddr));
    g_destAddr.sin_family = AF_INET;
    g_destAddr.sin_port   = htons(port);
    g_destAddr.sin_addr.s_addr = inet_addr(ip);
}

static void closeUDP()
{
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

static void initPBOs(int width, int height)
{
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

static void enqueueFrame(const unsigned char* ptr, int width, int height)
{
    const size_t size = static_cast<size_t>(width) * height * 4;
    std::vector<unsigned char> frame(size);
    std::memcpy(frame.data(), ptr, size);

    std::lock_guard<std::mutex> lock(g_frameMutex);
    g_frameQueue.push(std::move(frame));

    // Queue begrenzen, damit wir nicht hinterherhinken
    while (g_frameQueue.size() > 3) {
        g_frameQueue.pop();
    }
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

static void sendFrame(const std::vector<unsigned char>& frame)
{
    const int width  = g_width;
    const int height = g_height;
    const int rawSize = width * height * 4;

    // LZ4 komprimieren
    int maxCompressed = LZ4_compressBound(rawSize);
    std::vector<char> compressed(maxCompressed);

    int compressedSize = LZ4_compress_default(
        (const char*)frame.data(),
        compressed.data(),
        rawSize,
        maxCompressed
    );

    if (compressedSize <= 0)
        return;

    compressed.resize(compressedSize);

    // Fragmentierung
    const int MTU = 1300;  // sicher unter 1500
    const int headerSize = sizeof(FrameHeader);
    const int maxPayload = MTU - headerSize;

    int fragCount = (compressedSize + maxPayload - 1) / maxPayload;

    uint32_t frameID = g_frameCounter++;

    for (int i = 0; i < fragCount; i++) {

        int offset = i * maxPayload;
        int chunkSize = min(maxPayload, compressedSize - offset);

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
        sendto(g_sock, packet.data(), packet.size(), 0,
               (sockaddr*)&g_destAddr, sizeof(g_destAddr));
    }
}

// ------------------------------------------------------------
// Netzwerk‑Thread
// ------------------------------------------------------------

static void networkThreadLoop()
{
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

static GLuint getCurrentBoundTexture2D()
{
    GLint tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    return static_cast<GLuint>(tex);
}

// Panel‑Texturgröße automatisch ermitteln
static void updatePanelSizeFromTexture(GLuint tex)
{
    if (tex == 0) return;

    glBindTexture(GL_TEXTURE_2D, tex);

    GLint w = 0, h = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

    if (w > 0 && h > 0) {
        g_width  = w;
        g_height = h;
    }
}

// Panel capturen: GPU → PBO → Queue
static void capturePanel(GLuint texID)
{
    if (texID == 0)
        return;

    if (g_width == 0 || g_height == 0) {
        updatePanelSizeFromTexture(texID);
    }
    if (g_width == 0 || g_height == 0)
        return;

    if (!g_pboInitialized)
        initPBOs(g_width, g_height);

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

static int drawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon)
{

    static bool gladLoaded = false;
    if (!gladLoaded) {
        gladLoadGL();
        gladLoaded = true;
    }

    // Wir wollen NACH dem Panel‑Draw in der Gauges‑Phase
    if (inPhase != xplm_Phase_Gauges || inIsBefore)
        return 1;

    // In dieser Phase ist die Panel‑Textur gebunden
    GLuint panelTex = getCurrentBoundTexture2D();
    if (panelTex != 0) {
        capturePanel(panelTex);
    }

    return 1;
}

// -----------------------------
// Plugin‑Entry‑Points
// -----------------------------

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc)
{
    std::strcpy(outName, "Panelcast");
    std::strcpy(outSig,  "de.codenuts.panelcast");
    std::strcpy(outDesc, "Streaming von 2D‑Cockpit‑Panels über Netzwerk.");

    initUDP("127.0.0.1", 5000); // IP/Port anpassen
    
    XPLMRegisterDrawCallback(
        drawCallback,
        xplm_Phase_Gauges,
        0,      // after
        nullptr
    );

    g_running.store(true);
    g_networkThread = std::thread(networkThreadLoop);

    return 1;
}

PLUGIN_API void XPluginStop(void)
{
    XPLMUnregisterDrawCallback(
        drawCallback,
        xplm_Phase_Gauges,
        0,
        nullptr
    );

    g_running.store(false);
    if (g_networkThread.joinable())
        g_networkThread.join();

    closeUDP();
}

PLUGIN_API int XPluginEnable(void)
{
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam)
{
}
