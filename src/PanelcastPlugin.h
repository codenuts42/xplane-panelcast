/**
 * @file PanelcastPlugin.h
 * @brief High-level controller integrating all subsystems of the Panelcast plugin.
 *
 * The PanelcastPlugin class manages the lifecycle of the X‑Plane plugin and
 * coordinates the following subsystems:
 *
 *  - UDP networking (UdpSender)
 *  - framebuffer capture (PanelCapturer)
 *  - background compression and transmission (FrameSender)
 *  - X‑Plane draw callback registration
 *
 * This class acts as the central facade for the entire plugin.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <memory>

#include "ConfigManager.h"
#include "FrameSender.h"
#include "PanelCapturer.h"
#include "UdpSender.h"
#include "WsSender.h"

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"

/**
 * @class PanelcastPlugin
 * @brief Main façade class for the X‑Plane plugin lifecycle.
 *
 * Provides the required entry points for enabling, disabling, starting,
 * and stopping the plugin. Also registers the draw callback used to
 * capture panel framebuffer regions.
 */
class PanelcastPlugin {
  public:
	/**
	 * @brief Returns the global singleton instance of the plugin.
	 *
	 * X‑Plane plugins are typically implemented as singletons because
	 * the SDK expects global C‑style entry points.
	 */
	static PanelcastPlugin& instance();

	/**
	 * @brief Initializes all subsystems and registers the draw callback.
	 *
	 * Called once when the plugin is loaded by X‑Plane.
	 */
	bool start();

	/**
	 * @brief Shuts down all subsystems and unregisters callbacks.
	 *
	 * Called once when the plugin is unloaded by X‑Plane.
	 */
	void stop();

	/**
	 * @brief Called when the user enables the plugin in the Plugin Admin.
	 */
	int enable();

	/**
	 * @brief Called when the user disables the plugin in the Plugin Admin.
	 */
	void disable();

	/**
	 * @brief Static trampoline used by the X‑Plane SDK to call the instance method.
	 */
	static int drawCallbackTrampoline(XPLMDrawingPhase inPhase, int inIsBefore, void* refcon);

  private:
	PanelcastPlugin() = default;

	/**
	 * @brief Draw callback executed during X‑Plane's gauge rendering phase.
	 *
	 * Captures panel ROIs from the highest framebuffer object (FBO) to avoid
	 * capturing intermediate rendering passes.
	 */
	int drawCallback();

  private:
	ConfigManager config_;
	PanelCapturer panelCapturer_;
	UdpSender udpSender_;
	WsSender wsSender_;
	std::unique_ptr<FrameSender> frameSender_;
	bool useWebSocket_ = false;
};
