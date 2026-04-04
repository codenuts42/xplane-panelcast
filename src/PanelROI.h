/**
 * @file PanelROI.h
 * @brief Defines the rectangular regions of interest (ROIs) used for panel capture.
 *
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */
#pragma once
#include <cstdint>

/**
 * @brief Describes a rectangular capture region on the X‑Plane panel.
 */
struct PanelROI {
	uint16_t panelID;
	int x, y;
	int w, h;
};
