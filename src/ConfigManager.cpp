/**
 * @file ConfigManager.cpp
 * @brief Implementation of JSON configuration loading and hot‑reloading.
 */

#include "ConfigManager.h"
#include "Logger.h"

#include "XPLMPlugin.h"
#include "XPLMUtilities.h"

#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

static Logger logger("[Config]");

/**
 * @brief Default fallback configuration used when config.json is missing or invalid.
 */
static json defaultConfig = {
    {"udp", {{"ip", "127.0.0.1"}, {"port", 5000}}},
    {"panels", json::array({{{"id", "PFD"}, {"x", 5}, {"y", 1543}, {"w", 500}, {"h", 500}},
                            {{"id", "ND"}, {"x", 515}, {"y", 1543}, {"w", 500}, {"h", 500}}})}};

ConfigManager::ConfigManager() {
}

void ConfigManager::initialize() {
	detectPluginDirectory();

	json cfg = loadConfig();
	applyConfig(cfg);

	std::string path = getConfigPath();
	lastTimestamp_ = fs::exists(path) ? fs::last_write_time(path) : fs::file_time_type::min();
}

void ConfigManager::update() {
	std::string path = getConfigPath();

	if (!fs::exists(path))
		return;

	auto ts = fs::last_write_time(path);

	if (ts != lastTimestamp_) {
		lastTimestamp_ = ts;

		logger.log("Detected config.json change — reloading");

		json cfg = loadConfig();
		applyConfig(cfg);
	}
}

void ConfigManager::detectPluginDirectory() {
	char name[256] = {0};
	char filePath[512] = {0};
	char signature[256] = {0};
	char description[256] = {0};

	XPLMGetPluginInfo(XPLMGetMyID(), name, filePath, signature, description);

	fs::path p(filePath);
	pluginDirectory_ = p.parent_path().string();

	if (!pluginDirectory_.empty() && pluginDirectory_.back() != '/' && pluginDirectory_.back() != '\\') {
		pluginDirectory_ += "/";
	}

	logger.log("Plugin directory: {}", pluginDirectory_);
}

std::string ConfigManager::getConfigPath() const {
	return pluginDirectory_ + "config.json";
}

json ConfigManager::loadConfig() {
	std::string path = getConfigPath();

	try {
		if (!fs::exists(path)) {
			logger.log("config.json not found — using fallback defaults");
			return defaultConfig;
		}

		std::ifstream f(path);
		json cfg = json::parse(f);

		logger.log("Loaded config.json successfully");
		return cfg;

	} catch (const std::exception& e) {
		logger.log("Error parsing config.json: {}", e.what());
		logger.log("Using fallback default configuration");
		return defaultConfig;
	}
}

void ConfigManager::applyConfig(const json& cfg) {
	// UDP
	udpIP_ = cfg["udp"].value("ip", "127.0.0.1");
	udpPort_ = cfg["udp"].value("port", 5000);

	logger.log("UDP: {}:{}", udpIP_, udpPort_);

	// Panels
	panels_.clear();
	panelNameToID_.clear();

	uint16_t nextID = 0;

	for (const auto& p : cfg["panels"]) {
		std::string name = p.value("id", "UNKNOWN");
		uint16_t id = nextID++;

		panelNameToID_[name] = id;

		panels_.push_back({id, p.value("x", 0), p.value("y", 0), p.value("w", 100), p.value("h", 100)});

		logger.log("Panel {} → ID {} ({}x{} at {},{})", name, id, p.value("w", 0), p.value("h", 0), p.value("x", 0),
		           p.value("y", 0));
	}
}
