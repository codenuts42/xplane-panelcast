/**
 * @file RawPanelFrame.h
 * @brief Contains raw RGBA framebuffer data captured from a panel ROI.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <cstdint>
#include <vector>

/**
 * @brief Raw framebuffer data for a single captured panel region.
 *
 * The pixel buffer contains uncompressed RGBA8 data in row‑major order.
 */
struct RawPanelFrame {
	uint16_t panelID;
	int width;
	int height;
	std::vector<char> pixels;
};
