/**
 * @file PanelcastPlugin.cpp
 * @brief High-level integration of all Panelcast subsystems.
 *
 * Manages:
 *  - UDP networking
 *  - framebuffer capture
 *  - background compression/transmission
 *  - X‑Plane draw callback registration
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#include "PanelcastPlugin.h"
#include "Logger.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"

static std::vector<PanelROI> g_panels = {
    {0, 5, 1543, 500, 500},  // PFD
    {1, 515, 1543, 500, 500} // ND
};

PanelcastPlugin& PanelcastPlugin::instance() {
	static PanelcastPlugin inst;
	return inst;
}

bool PanelcastPlugin::start() {
	udpSender_.init("127.0.0.1", 5000);

#ifdef _WIN32
	gladLoadGL();
#endif

	frameSender_ = std::make_unique<FrameSender>(udpSender_);
	frameSender_->start();

	// Register draw callback for panel capture
	XPLMRegisterDrawCallback(drawCallbackTrampoline, xplm_Phase_Gauges, 0, this);

	return true;
}

void PanelcastPlugin::stop() {
	XPLMUnregisterDrawCallback(drawCallbackTrampoline, xplm_Phase_Gauges, 0, this);

	if (frameSender_) {
		frameSender_->stop();
		frameSender_.reset();
	}

	udpSender_.close();
}

int PanelcastPlugin::enable() {
	return 1;
}

void PanelcastPlugin::disable() {
}

int PanelcastPlugin::drawCallbackTrampoline(XPLMDrawingPhase inPhase, int inIsBefore, void* refcon) {
	return static_cast<PanelcastPlugin*>(refcon)->drawCallback();
}

/**
 * @brief Draw callback executed during X‑Plane's gauge rendering phase.
 *
 * Captures panel ROIs from the highest framebuffer object (FBO) to avoid
 * capturing intermediate rendering passes.
 */
int PanelcastPlugin::drawCallback() {
	static GLint maxFBO = 0;
	GLint currentFBO = 0;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
	if (currentFBO < maxFBO)
		return 1;

	maxFBO = currentFBO;

	// Capture framebuffer regions into FrameSender's buffer
	{
		auto lock = frameSender_->lockFrames();
		panelCapturer_.captureAllPanels(g_panels, frameSender_->frames());
	}

	return 1;
}
