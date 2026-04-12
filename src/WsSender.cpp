#include "WsSender.h"
#include "Logger.h"
#include <cstring>

static Logger logger("[WsSender]");

WsSender::WsSender() {
	logger.log("WsSender");
	mg_mgr_init(&mgr_);
}

WsSender::~WsSender() {
	logger.log("~WsSender");
	mg_mgr_free(&mgr_);
}

bool WsSender::initServer(const char* rootDir, const char* listenAddr) {
	logger.log("initServer rootDir={}", rootDir);
	logger.log("initServer listenAddr={}", listenAddr);

	webRoot_ = rootDir;

	mg_connection* c = mg_http_listen(&mgr_, listenAddr, httpHandler, this);
	if (!c) {
		logger.log("initServer FAILED: mg_http_listen returned null");
		return false;
	}

	logger.log("initServer OK: HTTP+WS listener created");
	return true;
}

void WsSender::pollOnce() {
	logger.log("pollOnce 1");
	mg_mgr_poll(&mgr_, 0);
	logger.log("pollOnce 2");
	if (!ws_) return;
	logger.log("pollOnce 3");
	std::lock_guard<std::mutex> lock(queueMutex_);
	int i = 1;
	for (auto& f : queue_) {
		logger.log("pollOnce 4: {}", i++);
		mg_ws_send(ws_, (const char*)f.data(), f.size(), WEBSOCKET_OP_BINARY);
	}
	queue_.clear();
}

void WsSender::sendFrame(uint16_t panelID, uint32_t frameID, const char* data, int size, int width, int height) {

	logger.log("sendFrame panelID={}, panelID={}", panelID, frameID);

	PanelFrameHeader hdr{};
	hdr.frameID = frameID;
	hdr.panelID = panelID;
	hdr.width = width;
	hdr.height = height;
	hdr.compressedSize = size;

	std::vector<uint8_t> packet(sizeof(hdr) + size);
	std::memcpy(packet.data(), &hdr, sizeof(hdr));
	std::memcpy(packet.data() + sizeof(hdr), data, size);

	std::lock_guard<std::mutex> lock(queueMutex_);
	if (queue_.size() > 2) { queue_.erase(queue_.begin(), queue_.end() - 2); }
	queue_.push_back(std::move(packet));
}

inline std::string to_string(mg_str s) {
	return std::string(s.buf, s.len);
}

static bool uriEquals(mg_str s, const char* cstr) {
	size_t n = strlen(cstr);
	return s.len == n && strncmp(s.buf, cstr, n) == 0;
}

void WsSender::httpHandler(mg_connection* c, int ev, void* ev_data) {
	auto* self = static_cast<WsSender*>(c->fn_data);

	if (ev == MG_EV_HTTP_MSG) {
		auto* hm = (mg_http_message*)ev_data;

		// 1) WebSocket-Upgrade auf /ws
		if (uriEquals(hm->uri, "/ws")) {
			logger.log("HTTP: WS upgrade on {}", to_string(hm->uri));
			mg_ws_upgrade(c, hm, nullptr);
			self->ws_ = c;
			return;
		}

		// 2) Statische Dateien ausliefern
		struct mg_http_serve_opts opts = {};
		opts.root_dir = self->webRoot_.c_str();

		logger.log("HTTP request: {} {}", to_string(hm->method), to_string(hm->uri));
		logger.log("Serving from: {}", opts.root_dir);

		mg_http_serve_dir(c, hm, &opts);
	}
}
