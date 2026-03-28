#include "Logger.h"

#include "XPLMPlugin.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>
#include <cstring>
#include <chrono>

// -----------------------------
// Globale Variablen
// -----------------------------

Logger logger("[Panelcast]");

static int g_width  = 0;
static int g_height = 0;

static GLuint g_pbo[2] = {0, 0};
static int g_pboIndex = 0;
static bool g_pboInitialized = false;

std::mutex g_frameMutex;
std::queue<std::vector<unsigned char>> g_frameQueue;

std::atomic<bool> g_running{false};
std::thread g_networkThread;

// -----------------------------
// Hilfsfunktionen
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

// Placeholder: hier später JPEG/H.264 + UDP einbauen
static void sendFrame(const std::vector<unsigned char>& frame)
{
    // TODO: Komprimieren + Netzwerk senden
}

// Netzwerk‑Thread: holt Frames aus Queue und verschickt sie
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

// Aktuell gebundene 2D‑Textur holen (Panel‑Textur im Gauges‑Pass)
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

    if (g_pboInitialized) {
        glDeleteBuffers(2, g_pbo);
        g_pboInitialized = false;
    }
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
