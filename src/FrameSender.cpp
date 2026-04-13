/**
 * @file FrameSender.cpp
 * @brief Implementation of the background worker for LZ4 compression and UDP transmission.
 *
 * The worker thread periodically consumes raw framebuffer captures,
 * compresses them using LZ4, and sends them via UdpSender.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#include "FrameSender.h"
#include "PanelFrameHeader.h"
#include <chrono>
#include <lz4.h>

FrameSender::FrameSender(FrameTransport& transport) : transport_(transport) {
}

FrameSender::~FrameSender() {
	stop();
}

void FrameSender::start() {
	running_.store(true);
	workerThread_ = std::thread(&FrameSender::workerLoop, this);
}

void FrameSender::stop() {
	running_.store(false);
	if (workerThread_.joinable()) workerThread_.join();
}

/**
 * @brief Main worker loop.
 *
 * Pulls all pending frames from the shared buffer (swap-based, non-blocking),
 * compresses each frame, and transmits it via UDP.
 */
void FrameSender::workerLoop() {
	while (running_.load()) {

		// Local buffer to avoid holding the mutex during compression
		std::unordered_map<uint16_t, RawPanelFrame> local;

		{
			// Swap out all pending frames in one atomic operation
			std::lock_guard<std::mutex> lock(frameMutex_);
			std::swap(local, frames_);
		}

		// Process all captured frames
		for (auto& [panelID, frame] : local)
			compressAndSendPanel(frame);

		// Prevent busy-looping
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

/**
 * @brief Compresses a single panel frame and sends it via UDP.
 *
 * Uses LZ4_fast for high-speed compression. Frames are fragmented into MTU-sized
 * UDP packets by UdpSender.
 */
void FrameSender::compressAndSendPanel(const RawPanelFrame& f) {
	int rawSize = f.width * f.height * 4;

	int maxComp = LZ4_compressBound(rawSize);
	std::vector<char> comp(maxComp);
	int compSize = LZ4_compress_fast(f.pixels.data(), comp.data(), rawSize, maxComp, 8);
	if (compSize <= 0) return;
	comp.resize(compSize);

	uint32_t& counter = frameCounters_[f.panelID];
	uint32_t frameID = counter++;

	transport_.sendFrame(f.panelID, frameID, comp.data(), compSize, f.width, f.height);
}
