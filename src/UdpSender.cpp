/**
 * @file UdpSender.cpp
 * @brief Implementation of the UDP transmission backend for Panelcast.
 *
 * Handles platform‑independent UDP socket creation and sending of fragmented
 * compressed panel frames. Used by FrameSender.
 *
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
 */

#include "UdpSender.h"
#include <cstring>

UdpSender::~UdpSender() {
	close();
}

bool UdpSender::init(const char* ip, uint16_t port) {
#ifdef _WIN32
	// Initialize Winsock on Windows
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	// Create UDP socket
	socket_ = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_ < 0)
		return false;

	// Configure destination address
	memset(&destAddr_, 0, sizeof(destAddr_));
	destAddr_.sin_family = AF_INET;
	destAddr_.sin_port = htons(port);
	inet_pton(AF_INET, ip, &destAddr_.sin_addr);

	return true;
}

void UdpSender::close() {
	if (socket_ >= 0) {
#ifdef _WIN32
		closesocket(socket_);
		WSACleanup();
#else
		::close(socket_);
#endif
		socket_ = -1;
	}
}

/**
 * @brief Header structure prepended to each UDP fragment.
 *
 * Contains metadata required by the receiver to reconstruct the full frame.
 */
struct PanelFragmentHeader {
	uint32_t magic;
	uint32_t frameID;
	uint16_t panelID;
	uint16_t fragIndex;
	uint16_t fragCount;
	uint16_t panelCount;
	uint32_t payloadSize;
	uint16_t width;
	uint16_t height;
	uint32_t compressedSize;
};

void UdpSender::sendPanelFragments(uint16_t panelID, uint32_t frameID, const char* data, int size, int w, int h) {
	if (socket_ < 0)
		return;

	const int mtu = 1472;
	const int headerSize = sizeof(PanelFragmentHeader);
	const int maxPayload = mtu - headerSize; // safe MTU

	int fragCount = (size + maxPayload - 1) / maxPayload;

	PanelFragmentHeader hdr{};
	hdr.magic = 0xABCD1234;
	hdr.frameID = frameID;
	hdr.panelID = panelID;
	hdr.fragCount = fragCount;
	hdr.width = w;
	hdr.height = h;
	hdr.compressedSize = size;

	// Send each fragment
	for (int i = 0; i < fragCount; i++) {
		int offset = i * maxPayload;
		int chunkSize = std::min(maxPayload, size - offset);

		hdr.fragIndex = i;
		hdr.payloadSize = chunkSize;

		// Build packet buffer
		char buffer[mtu];
		memcpy(buffer, &hdr, headerSize);
		memcpy(buffer + headerSize, data + offset, chunkSize);

		// Transmit
		sendto(socket_, buffer, headerSize + chunkSize, 0, (sockaddr*)&destAddr_, sizeof(destAddr_));
	}
}
