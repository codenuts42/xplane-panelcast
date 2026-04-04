/**
 * @file FrameSender.cpp
 * @brief Implementation of the background worker for LZ4 compression and UDP transmission.
 *
 * The worker thread periodically consumes raw framebuffer captures,
 * compresses them using LZ4, and sends them via UdpSender.
 *
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */

#include "FrameSender.h"
#include <chrono>
#include <lz4.h>

FrameSender::FrameSender(UdpSender& sender) : udpSender_(sender) {}

FrameSender::~FrameSender() {
	stop();
}

void FrameSender::start() {
	running_.store(true);
	workerThread_ = std::thread(&FrameSender::workerLoop, this);
}

void FrameSender::stop() {
	running_.store(false);
	if (workerThread_.joinable())
		workerThread_.join();
}

std::unordered_map<uint16_t, RawPanelFrame>& FrameSender::getFrameMap() {
	return latestFrames_;
}

std::mutex& FrameSender::getMutex() {
	return framesMutex_;
}

void FrameSender::workerLoop() {
	while (running_.load()) {
		std::unordered_map<uint16_t, RawPanelFrame> frames;

		// Swap out the latest frames
		{
			std::lock_guard<std::mutex> lock(framesMutex_);
			frames = latestFrames_;
			latestFrames_.clear();
		}

		// Compress and send each frame
		for (auto& [panelID, frame] : frames) compressAndSendPanel(frame);

		// Avoid busy‑looping
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void FrameSender::compressAndSendPanel(const RawPanelFrame& f) {
	int rawSize = f.width * f.height * 4;

	// Allocate compression buffer
	int maxComp = LZ4_compressBound(rawSize);
	std::vector<char> comp(maxComp);

	// Compress raw RGBA data
	int compSize = LZ4_compress_fast(f.pixels.data(), comp.data(), rawSize, maxComp, 8);

	if (compSize <= 0)
		return;

	comp.resize(compSize);

	uint32_t frameID = frameCounter_++;

	// Send via UDP
	udpSender_.sendPanelFragments(f.panelID, frameID, comp.data(), compSize, f.width, f.height);
}
