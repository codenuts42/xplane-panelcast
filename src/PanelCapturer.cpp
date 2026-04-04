/**
 * @file PanelCapturer.cpp
 * @brief Implementation of OpenGL PBO‑based framebuffer capture.
 *
 * Uses double‑buffered Pixel Buffer Objects to asynchronously read
 * framebuffer regions (ROIs) from X‑Plane's panel rendering phase.
 *
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */

#include "PanelCapturer.h"
#include <cstring>

void PanelCapturer::initOrResizePanelPBOs(uint16_t panelID, int w, int h) {
	auto& st = panelStates[panelID];

	// If already initialized with correct size, nothing to do
	if (st.initialized && st.w == w && st.h == h)
		return;

	// Delete old PBOs if necessary
	if (st.initialized)
		glDeleteBuffers(2, st.pbo);

	// Allocate new double‑buffered PBOs
	glGenBuffers(2, st.pbo);

	for (int i = 0; i < 2; i++) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, st.pbo[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, w * h * 4, nullptr, GL_STREAM_READ);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	st.initialized = true;
	st.pboIndex = 0;
	st.w = w;
	st.h = h;
}

void PanelCapturer::captureSinglePanel(const PanelROI& roi, std::unordered_map<uint16_t, RawPanelFrame>& outFrames,
                                       std::mutex& outMutex) {
	int w = roi.w;
	int h = roi.h;

	auto& st = panelStates_[roi.panelID];
	initOrResizePanelPBOs(roi.panelID, w, h);

	int next = (st.pboIndex + 1) % 2;

	// 1. Issue GPU readback into current PBO
	glBindBuffer(GL_PIXEL_PACK_BUFFER, st.pbo[st.pboIndex]);
	glReadPixels(roi.x, roi.y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// 2. Map previous PBO to retrieve completed data
	glBindBuffer(GL_PIXEL_PACK_BUFFER, st.pbo[next]);
	unsigned char* ptr = (unsigned char*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

	if (ptr) {
		RawPanelFrame frame;
		frame.panelID = roi.panelID;
		frame.width = w;
		frame.height = h;
		frame.pixels.resize(w * h * 4);

		memcpy(frame.pixels.data(), ptr, w * h * 4);

		{
			std::lock_guard<std::mutex> lock(outMutex);
			outFrames[roi.panelID] = std::move(frame);
		}

		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	st.pboIndex = next;
}

void PanelCapturer::captureAllPanels(const std::vector<PanelROI>& rois,
                                     std::unordered_map<uint16_t, RawPanelFrame>& outFrames, std::mutex& outMutex) {
	for (const auto& roi : rois) captureSinglePanel(roi, outFrames, outMutex);
}
