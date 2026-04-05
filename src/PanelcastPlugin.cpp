/**
 * @file PanelcastPlugin.cpp
 * @brief Integration of all Panelcast subsystems.
 */

#include "PanelcastPlugin.h"

PanelcastPlugin& PanelcastPlugin::instance() {
	static PanelcastPlugin inst;
	return inst;
}

bool PanelcastPlugin::start() {
	config_.initialize();

	udpSender_.init(config_.udpIP().c_str(), config_.udpPort());

#ifdef _WIN32
	gladLoadGL();
#endif

	frameSender_ = std::make_unique<FrameSender>(udpSender_);
	frameSender_->start();

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

int PanelcastPlugin::drawCallback() {
	config_.update();

	static GLint maxFBO = 0;
	GLint currentFBO = 0;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
	if (currentFBO < maxFBO)
		return 1;

	maxFBO = currentFBO;

	{
		auto lock = frameSender_->lockFrames();
		panelCapturer_.captureAllPanels(config_.panels(), frameSender_->frames());
	}

	return 1;
}
