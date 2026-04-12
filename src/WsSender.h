/**
 * @file WsSender.h
 * @brief WebSocket transmission backend for Panelcast.
 */

#pragma once
#include "FrameTransport.h"
#include "PanelFrameHeader.h"
#include "mongoose.h"
#include <mutex>
#include <vector>

class WsSender : public FrameTransport {
  public:
	WsSender();
	~WsSender();

	bool initServer(const char* rootDir, const char* listenAddr);

	void pollOnce();

	void sendFrame(uint16_t panelID, uint32_t frameID, const char* data, int size, int width, int height) override;

  private:
	mg_mgr mgr_{};
	mg_connection* ws_ = nullptr;

	std::mutex queueMutex_;
	std::vector<std::vector<uint8_t>> queue_;

	std::string webRoot_;

	static void httpHandler(mg_connection* c, int ev, void* ev_data);
};
