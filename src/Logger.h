/**
 * @file Logger.h
 * @brief Lightweight wrapper for X‑Plane's debug logging facility.
 *
 * Provides formatted logging using std::format and forwards messages to
 * XPLMDebugString(). Automatically appends a newline if missing.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include "XPLMUtilities.h"
#include <format>
#include <string>
#include <string_view>

/**
 * @brief Simple formatted logger for X‑Plane plugin debugging.
 *
 * Each Logger instance has a prefix (e.g., "[Panelcast]") that is prepended
 * to every message. Logging uses std::format and std::vformat internally.
 */
class Logger {
  public:
	explicit Logger(std::string prefix) : prefix_(std::move(prefix)) {
	}

	/**
	 * @brief Logs a formatted message to X‑Plane's debug console.
	 *
	 * Automatically appends a newline if the message does not end with one.
	 */
	template <typename... Args> void log(std::string_view fmt, Args&&... args) const {
		std::string msg = std::format("{} {}", prefix_, std::vformat(fmt, std::make_format_args(args...)));

		if (!msg.ends_with('\n'))
			msg.push_back('\n');

		XPLMDebugString(msg.c_str());
	}

  private:
	std::string prefix_;
};
