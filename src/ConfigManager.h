/**
 * @file ConfigManager.h
 * @brief Handles loading, caching, and hot‑reloading of config.json.
 *
 * Responsibilities:
 *  - Determine plugin directory via XPLMGetPluginInfo()
 *  - Cache config.json path
 *  - Load JSON configuration (with fallback)
 *  - Detect file changes and hot‑reload
 *  - Provide parsed UDP + panel configuration
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "PanelROI.h"

/**
 * @class ConfigManager
 * @brief Central configuration handler for the Panelcast plugin.
 *
 * Loads and manages the JSON configuration file, including fallback defaults
 * and automatic hot‑reloading when the file changes on disk.
 */
class ConfigManager {
  public:
	enum class TransportMode { Udp, WebSocket };

	TransportMode transportMode() const {
		// TODO: aus config.json lesen
		return TransportMode::WebSocket;
	}

	std::string httpUrl() const {
		return "http://0.0.0.0:" + std::to_string(webPort_);
	}

	std::string getWebPath() const;

	/**
	 * @brief Constructs an empty ConfigManager.
	 */
	ConfigManager();

	/**
	 * @brief Initializes plugin directory and loads initial configuration.
	 *
	 * Must be called once during plugin startup.
	 */
	void initialize();

	/**
	 * @brief Checks whether config.json changed and reloads it if necessary.
	 *
	 * Should be called once per frame (e.g., in draw callback).
	 */
	void update();

	/**
	 * @brief Returns parsed panel ROIs.
	 */
	const std::vector<PanelROI>& panels() const {
		return panels_;
	}

	/**
	 * @brief Returns mapping from panel name → numeric ID.
	 */
	const std::unordered_map<std::string, uint16_t>& panelNameToID() const {
		return panelNameToID_;
	}

	/**
	 * @brief Returns configured UDP IP address.
	 */
	const std::string& udpIP() const {
		return udpIP_;
	}

	/**
	 * @brief Returns configured UDP port.
	 */
	uint16_t udpPort() const {
		return udpPort_;
	}

	/**
	 * @brief Returns configured WebSocket port.
	 */
	uint16_t webPort() const {
		return webPort_;
	}

  private:
	/**
	 * @brief Determines the plugin directory using XPLMGetPluginInfo().
	 */
	void detectPluginDirectory();

	/**
	 * @brief Returns the full path to config.json.
	 */
	std::string getConfigPath() const;

	/**
	 * @brief Loads config.json or returns fallback configuration.
	 */
	nlohmann::json loadConfig();

	/**
	 * @brief Applies loaded JSON configuration to internal state.
	 */
	void applyConfig(const nlohmann::json& cfg);

  private:
	std::string pluginDirectory_;                             ///< Cached plugin directory path
	std::filesystem::file_time_type lastTimestamp_;           ///< Last modification time of config.json
	std::vector<PanelROI> panels_;                            ///< Parsed panel definitions
	std::unordered_map<std::string, uint16_t> panelNameToID_; ///< Mapping name → ID
	std::string udpIP_;                                       ///< Configured UDP IP
	uint16_t udpPort_ = 0;                                    ///< Configured UDP port
	uint16_t webPort_ = 0;                                    ///< Configured WebService port
};
