/**
 * @file PanelcastPlugin.h
 * @brief High‑level controller integrating all subsystems of the Panelcast plugin.
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
#pragma once
#include <memory>
#include <vector>

#include "FrameSender.h"
#include "PanelCapturer.h"
#include "PanelROI.h"
#include "UdpSender.h"
#include "XPLMDisplay.h"

/**
 * @brief Main facade class for the X‑Plane plugin lifecycle.
 */
class PanelcastPlugin {
  public:
	static PanelcastPlugin& instance();

	bool start();
	void stop();

	int enable();
	void disable();

	static int drawCallbackTrampoline(XPLMDrawingPhase inPhase, int inIsBefore, void* refcon);

  private:
	PanelcastPlugin() = default;

	int drawCallback();

	UdpSender udpSender;
	PanelCapturer panelCapturer;
	std::unique_ptr<FrameSender> frameSender;
};
