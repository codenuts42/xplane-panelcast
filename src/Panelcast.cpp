/**
 * @file Panelcast.cpp
 * @brief X‑Plane plugin entry points for the Panelcast plugin.
 *
 * This file contains the C‑style entry functions required by the X‑Plane SDK.
 * All actual plugin logic is implemented inside PanelcastPlugin.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#include "PanelcastPlugin.h"
#include <cstring>

/**
 * @brief Called once when X‑Plane loads the plugin.
 *
 * Must return 1 on success, otherwise X‑Plane refuses to load the plugin.
 */
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
	std::strcpy(outName, "Panelcast");
	std::strcpy(outSig, "de.codenuts.panelcast");
	std::strcpy(outDesc, "Streaming of cockpit panel regions via UDP");

	return PanelcastPlugin::instance().start() ? 1 : 0;
}

/**
 * @brief Called once when X‑Plane unloads the plugin.
 */
PLUGIN_API void XPluginStop(void) {
	PanelcastPlugin::instance().stop();
}

/**
 * @brief Called when the user enables the plugin in the Plugin Admin.
 */
PLUGIN_API int XPluginEnable(void) {
	return PanelcastPlugin::instance().enable();
}

/**
 * @brief Called when the user disables the plugin in the Plugin Admin.
 */
PLUGIN_API void XPluginDisable(void) {
	PanelcastPlugin::instance().disable();
}

/**
 * @brief Receives messages from X‑Plane or other plugins.
 *
 * Currently unused, but required by the API.
 */
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {
	// No message handling implemented
}
