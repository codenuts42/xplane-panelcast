/**
 * @file PanelcastPlugin.cpp
 * @brief High‑level integration of all Panelcast subsystems.
 *
 * Manages:
 *  - UDP networking
 *  - framebuffer capture
 *  - background compression/transmission
 *  - X‑Plane draw callback registration
 *
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
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
void PanelcastPlugin::disable() {}

int PanelcastPlugin::drawCallbackTrampoline(XPLMDrawingPhase inPhase, int inIsBefore, void* refcon) {
	return static_cast<PanelcastPlugin*>(refcon)->drawCallback();
}

int PanelcastPlugin::drawCallback() {
	static GLint maxFBO = 0;
	GLint currentFBO = 0;

	// Only capture from the highest FBO (X‑Plane draws multiple passes)
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
	if (currentFBO < maxFBO)
		return 1;

	maxFBO = currentFBO;

	// Perform capture
	panelCapturer_.captureAllPanels(g_panels, frameSender_->getFrameMap(), frameSender_->getMutex());

	return 1;
}
