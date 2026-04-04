/**
 * @file FrameSender.h
 * @brief Background worker for LZ4 compression and UDP transmission.
 *
 * Consumes raw frames produced by PanelCapturer and sends them via UdpSender.
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */
#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "RawPanelFrame.h"
#include "UdpSender.h"

struct FrameBuffer {
	std::mutex mtx;
	std::unordered_map<uint16_t, RawPanelFrame> frames;
};

/**
 * @brief Worker thread that compresses and transmits panel frames.
 */
class FrameSender {
  public:
	explicit FrameSender(UdpSender& sender);
	~FrameSender();

	void start();
	void stop();

	// Kombinierter Zugriff
	std::unique_lock<std::mutex> lockFrames() {
		return std::unique_lock<std::mutex>(frameBuffer_.mtx);
	}

	std::unordered_map<uint16_t, RawPanelFrame>& frames() {
		return frameBuffer_.frames;
	}

  private:
	void workerLoop();
	void compressAndSendPanel(const RawPanelFrame& f);

	UdpSender& udpSender_;
	std::atomic<bool> running_{false};
	std::thread workerThread_;

	FrameBuffer frameBuffer_;
	uint32_t frameCounter_ = 0;
};
