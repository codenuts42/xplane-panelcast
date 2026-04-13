/**
 * @file FrameSender.h
 * @brief Background worker that compresses captured panel frames and transmits them via UDP.
 *
 * The FrameSender owns the shared framebuffer buffer, protects it with a mutex,
 * and runs a worker thread that periodically consumes the captured frames,
 * compresses them using LZ4, and sends them through the UdpSender.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "FrameTransport.h"
#include "RawPanelFrame.h"

/**
 * @brief Worker thread that compresses and transmits panel frames.
 *
 * The FrameSender receives raw frames from the PanelCapturer, stores them in
 * a synchronized buffer, and asynchronously processes them in a background
 * thread. Each frame is LZ4‑compressed and fragmented into UDP packets.
 */
class FrameSender {
  public:
	FrameSender(FrameTransport& transport);
	~FrameSender();

	void start();
	void stop();

	std::unique_lock<std::mutex> lockFrames() {
		return std::unique_lock<std::mutex>(frameMutex_);
	}

	std::unordered_map<uint16_t, RawPanelFrame>& frames() {
		return frames_;
	}

  private:
	void workerLoop();
	void compressAndSendPanel(const RawPanelFrame& f);

	FrameTransport& transport_;

	std::atomic<bool> running_{false};
	std::thread workerThread_;

	std::mutex frameMutex_;
	std::unordered_map<uint16_t, RawPanelFrame> frames_;
	std::unordered_map<uint16_t, uint32_t> frameCounters_;
};
