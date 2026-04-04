/**
 * @file PanelCapturer.h
 * @brief Captures framebuffer regions using OpenGL Pixel Buffer Objects (PBOs).
 *
 * The PanelCapturer reads rectangular regions of the X‑Plane panel framebuffer
 * using double‑buffered PBOs to achieve asynchronous GPU readback. Captured
 * pixel data is written into RawPanelFrame objects.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <glad/glad.h>
#include <unordered_map>
#include <vector>

#include "PanelROI.h"
#include "RawPanelFrame.h"

/**
 * @brief Internal state for a panel's double‑buffered PBO capture.
 *
 * Each panel ROI maintains two PBOs that are alternated every frame to allow
 * asynchronous readback without stalling the GPU pipeline.
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
 *
 * The caller must ensure external synchronization when writing into the
 * output frame map. The capturer itself performs no locking.
 */
class PanelCapturer {
  public:
	void captureAllPanels(const std::vector<PanelROI>& rois, std::unordered_map<uint16_t, RawPanelFrame>& outFrames);

  private:
	void initOrResizePanelPBOs(uint16_t panelID, int w, int h);
	void captureSinglePanel(const PanelROI& roi, std::unordered_map<uint16_t, RawPanelFrame>& outFrames);

	std::unordered_map<uint16_t, PanelCaptureState> panelStates_;
};
