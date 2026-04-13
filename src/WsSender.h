/**
 * @file WsSender.h
 * @brief Minimal threaded WebSocket backend for Panelcast.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include "FrameTransport.h"
#include "PanelFrameHeader.h"
#include "mongoose.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

class WsSender : public FrameTransport {
  public:
	WsSender();
	~WsSender();

	bool init(std::string rootDir, const char* listenAddr);

	// Wird 1× pro Frame im X‑Plane drawCallback aufgerufen
	void doPoll(int timeoutMs);

	void sendFrame(uint16_t panelID, uint32_t frameID, const char* data, int size, int width, int height) override;

  private:
	static void httpHandler(mg_connection* c, int ev, void* ev_data);

	void threadMain();

  private:
	mg_mgr mgr_{};
	mg_connection* ws_ = nullptr;

	std::string webRoot_;

	std::mutex queueMutex_;
	std::vector<uint8_t> latestFrame_; // immer nur 1 Frame

	bool running_ = false;
};
