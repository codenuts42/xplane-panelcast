#pragma once
#include "XPLMUtilities.h"
#include <format>
#include <string>
#include <string_view>

class Logger {
  public:
	explicit Logger(std::string prefix) : prefix_(std::move(prefix)) {}

	template <typename... Args> void log(std::string_view fmt, Args&&... args) const {
		std::string msg = std::format("{} {}", prefix_, std::vformat(fmt, std::make_format_args(args...)));

		if (!msg.ends_with('\n'))
			msg.push_back('\n');

		XPLMDebugString(msg.c_str());
	}

  private:
	std::string prefix_;
};
