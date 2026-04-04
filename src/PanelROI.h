/**
 * @file PanelROI.h
 * @brief Defines rectangular regions of interest (ROIs) for panel capture.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <cstdint>

/**
 * @brief Describes a rectangular capture region on the X‑Plane panel.
 *
 * Each ROI identifies a panel by ID and specifies the pixel coordinates
 * and dimensions of the region to be captured.
 */
struct PanelROI {
	uint16_t panelID;
	int x, y;
	int w, h;
};
