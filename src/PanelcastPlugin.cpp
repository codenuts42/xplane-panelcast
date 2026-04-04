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

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::vector<PanelROI> g_panels = {
    {0, 5, 1543, 500, 500},  // PFD
    {1, 515, 1543, 500, 500} // ND
};

PanelcastPlugin& PanelcastPlugin::instance() {
	static PanelcastPlugin inst;
	return inst;
}

bool PanelcastPlugin::start() {

	// Load JSON config
	std::ifstream f("Resources/plugins/Panelcast/config.json");
	json cfg = json::parse(f);

	// UDP init from config
	std::string ip = cfg["udp"]["ip"];
	uint16_t port = cfg["udp"]["port"];
	udpSender_.init(ip.c_str(), port);

#ifdef _WIN32
	gladLoadGL();
#endif

	// Build panel list
	const auto& arr = cfg["panels"];
	uint16_t nextID = 0;

	panels_.clear();
	panelNameToID_.clear();

	for (const auto& p : arr) {
		std::string name = p["id"];
		uint16_t id = nextID++;

		panelNameToID_[name] = id;

		panels_.push_back({id, p["x"], p["y"], p["w"], p["h"]});
	}

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
		panelCapturer_.captureAllPanels(panels_, frameSender_->frames());
	}

	return 1;
}
