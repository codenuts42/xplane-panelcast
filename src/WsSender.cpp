/**
 * @file WsSender.cpp
 * @brief Minimal threaded WebSocket backend for Panelcast.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#include "WsSender.h"
#include "Logger.h"
#include <chrono>
#include <cstring>

static Logger logger("[WsSender]");

WsSender::WsSender() {
	logger.log("WsSender()");
}

WsSender::~WsSender() {
	logger.log("~WsSender()");
	running_ = false;
	mg_mgr_free(&mgr_);
}

bool WsSender::init(std::string rootDir, const char* listenAddr) {
	logger.log("initServer rootDir={}", rootDir);
	logger.log("initServer listenAddr={}", listenAddr);

	webRoot_ = rootDir;

	mg_mgr_init(&mgr_);

	mg_connection* c = mg_http_listen(&mgr_, listenAddr, httpHandler, this);
	if (!c) {
		logger.log("ERROR: Cannot listen on {}", listenAddr);
		return false;
	}

	running_ = true;
	logger.log("Server started on {}", listenAddr);
	return true;
}

static bool uriEquals(mg_str s, const char* cstr) {
	size_t n = strlen(cstr);
	return s.len == n && strncmp(s.buf, cstr, n) == 0;
}

void WsSender::httpHandler(mg_connection* c, int ev, void* ev_data) {
	auto* self = static_cast<WsSender*>(c->fn_data);

	switch (ev) {

	case MG_EV_HTTP_MSG: {
		auto* hm = (mg_http_message*)ev_data;

		// 1) WebSocket-Upgrade?
		if (uriEquals(hm->uri, "/ws")) {
			mg_ws_upgrade(c, hm, nullptr);
			return;
		}

		// 2) Normale Dateien ausliefern
		struct mg_http_serve_opts opts = {};
		opts.root_dir = self->webRoot_.c_str();
		mg_http_serve_dir(c, hm, &opts);
		return;
	}

	case MG_EV_WS_OPEN: {
		logger.log("Websocket connected");
		self->ws_ = c;
		return;
	}

	case MG_EV_CLOSE: {
		if (self->ws_ == c) {
			logger.log("Websocket closed");
			self->ws_ = nullptr;
		}
		return;
	}
	}
}

static void rgbaToRgb565(const uint8_t* src, uint16_t* dst, int pixelCount) {
	for (int i = 0; i < pixelCount; i++) {
		uint8_t r = src[i * 4 + 0] >> 3; // 5 Bit
		uint8_t g = src[i * 4 + 1] >> 2; // 6 Bit
		uint8_t b = src[i * 4 + 2] >> 3; // 5 Bit

		dst[i] = (r << 11) | (g << 5) | b;
	}
}

void WsSender::sendFrame(uint16_t panelID, uint32_t frameID, const char* data, int size, int width, int height) {
	int pixelCount = width * height;
	int rgb565Size = pixelCount * 2;

	PanelFrameHeader hdr;
	hdr.magic = 0xABCD1234;
	hdr.frameID = frameID;
	hdr.panelID = panelID;
	hdr.width = width;
	hdr.height = height;
	hdr.compressedSize = rgb565Size;

	std::vector<uint8_t> packet(sizeof(PanelFrameHeader) + rgb565Size);
	memcpy(packet.data(), &hdr, sizeof(PanelFrameHeader));

	// RGBA → RGB565
	rgbaToRgb565((const uint8_t*)data, (uint16_t*)(packet.data() + sizeof(PanelFrameHeader)), pixelCount);

	std::lock_guard<std::mutex> lock(queueMutex_);
	latestFrame_ = std::move(packet); // immer nur der neueste Frame
}

void WsSender::doPoll(int timeoutMs) {
	if (!running_) return;

	mg_mgr_poll(&mgr_, timeoutMs);

	mg_connection* ws = ws_;
	if (!ws) return;

	// Wenn Socket ausgelastet → nichts senden
	if (ws->send.len > 0) return;

	std::vector<uint8_t> frame;
	{
		std::lock_guard<std::mutex> lock(queueMutex_);
		if (latestFrame_.empty()) return;
		frame = latestFrame_; // Kopie
	}

	mg_ws_send(ws, (const char*)frame.data(), frame.size(), WEBSOCKET_OP_BINARY);
}