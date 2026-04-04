/**
 * @file RawPanelFrame.h
 * @brief Contains the raw framebuffer data captured from a panel ROI.
 *
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */
#pragma once
#include <cstdint>
#include <vector>

/**
 * @brief Raw RGBA framebuffer data for a single panel capture.
 */
struct RawPanelFrame {
	uint16_t panelID;
	int width;
	int height;
	std::vector<char> pixels;
};
