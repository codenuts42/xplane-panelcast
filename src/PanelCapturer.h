/**
 * @file PanelCapturer.h
 * @brief Implements OpenGL PBO‑based framebuffer capture for panel ROIs.
 *
 * Uses double‑buffered Pixel Buffer Objects for asynchronous GPU readback.
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */
#pragma once
#include <glad/glad.h>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "PanelROI.h"
#include "RawPanelFrame.h"

/**
 * @brief Internal state for a panel's double‑buffered PBO capture.
 */
struct PanelCaptureState {
	GLuint pbo[2] = {0, 0};
	int pboIndex = 0;
	bool initialized = false;
	int w = 0;
	int h = 0;
};

/**
 * @brief Captures framebuffer regions using OpenGL PBOs.
 */
class PanelCapturer {
  public:
	void captureAllPanels(const std::vector<PanelROI>& rois, std::unordered_map<uint16_t, RawPanelFrame>& outFrames,
	                      std::mutex& outMutex);

  private:
	void initOrResizePanelPBOs(uint16_t panelID, int w, int h);
	void captureSinglePanel(const PanelROI& roi, std::unordered_map<uint16_t, RawPanelFrame>& outFrames,
	                        std::mutex& outMutex);

	std::unordered_map<uint16_t, PanelCaptureState> panelStates;
};
