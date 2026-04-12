// FrameTransport.h
#pragma once
#include <cstdint>

struct FrameTransport {
	virtual ~FrameTransport() = default;

	virtual void sendFrame(uint16_t panelID, uint32_t frameID, const char* data, int size, int width, int height) = 0;
};
