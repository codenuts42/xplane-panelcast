#pragma once
#include "XPLMUtilities.h"
#include <cstdarg>
#include <cstdio>
#include <string>

class Logger {
public:
    explicit Logger(const char* prefix)
        : prefix_(prefix) {}

    void log(const char* fmt, ...) const {
        char buffer[512];

        int prefixLen = snprintf(buffer, sizeof(buffer), "%s ", prefix_);

        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, fmt, args);
        va_end(args);

        std::string msg(buffer);
        if (msg.back() != '\n')
            msg.push_back('\n');

        XPLMDebugString(msg.c_str());
    }

private:
    const char* prefix_;
};
