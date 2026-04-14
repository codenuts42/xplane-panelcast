/**
 * @file RawPanelFrame.h
 * @brief Contains raw RGBA framebuffer data captured from a panel ROI.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <cstdint>
#include <vector>

struct RawPanelFrame {
	uint16_t panelID;
	int width;
	int height;

	// RGBA8, row-major, 4 bytes per pixel
	std::vector<uint8_t> pixels;
};
