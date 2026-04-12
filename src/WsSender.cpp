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

// Helpers
inline std::string to_string(mg_str s) {
	return std::string(s.buf, s.len);
}

static bool uriEquals(mg_str s, const char* cstr) {
	size_t n = strlen(cstr);
	return s.len == n && strncmp(s.buf, cstr, n) == 0;
}

WsSender::WsSender() {
	logger.log("WsSender()");
}

WsSender::~WsSender() {
	logger.log("~WsSender()");
	running_ = false;
	if (thread_.joinable()) thread_.join();
}

bool WsSender::initServer(const char* rootDir, const char* listenAddr) {
	logger.log("initServer rootDir={}", rootDir);
	logger.log("initServer listenAddr={}", listenAddr);

	webRoot_ = rootDir;

	mg_mgr_init(&mgr_);

	mg_connection* c = mg_http_listen(&mgr_, listenAddr, httpHandler, this);
	if (!c) {
		logger.log("ERROR: Cannot listen on {}", listenAddr);
		return false;
	}

	// Thread starten
	running_ = true;
	thread_ = std::thread(&WsSender::threadMain, this);

	logger.log("Server started on {}", listenAddr);
	return true;
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
	hdr.compressedSize = rgb565Size; // WICHTIG: neue Größe

	std::vector<uint8_t> packet(sizeof(PanelFrameHeader) + rgb565Size);

	memcpy(packet.data(), &hdr, sizeof(PanelFrameHeader));

	// RGBA → RGB565
	rgbaToRgb565((const uint8_t*)data, (uint16_t*)(packet.data() + sizeof(PanelFrameHeader)), pixelCount);

	// ALWAYS-LATEST
	std::lock_guard<std::mutex> lock(queueMutex_);
	queue_.clear();
	queue_.push_back(std::move(packet));
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
		printf("[WsSender] WS connected\n");
		self->ws_ = c;
		return;
	}

	case MG_EV_CLOSE: {
		if (self->ws_ == c) {
			printf("[WsSender] WS closed\n");
			self->ws_ = nullptr;
		}
		return;
	}
	}
}

void WsSender::threadMain() {
	const int targetFps = 20; // perfekt für alte iPads
	const int frameIntervalMs = 1000 / targetFps;

	auto lastSend = std::chrono::steady_clock::now();

	while (running_) {
		mg_mgr_poll(&mgr_, 10); // 10ms Poll

		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSend).count();
		if (elapsed < frameIntervalMs) continue;

		mg_connection* ws = ws_;
		if (!ws) continue;

		// Wenn Socket ausgelastet → Frame verwerfen
		if (ws->send.len > 0) continue;

		std::vector<uint8_t> frame;
		{
			std::lock_guard<std::mutex> lock(queueMutex_);
			if (queue_.empty()) continue;
			frame = queue_.back(); // immer nur den neuesten Frame
		}

		mg_ws_send(ws, (const char*)frame.data(), frame.size(), WEBSOCKET_OP_BINARY);
		lastSend = now;
	}

	mg_mgr_free(&mgr_);
}
