/**
 * @file UdpSender.cpp
 * @brief Implementation of the UDP transmission backend for Panelcast.
 *
 * Handles platform-independent UDP socket creation and sending of fragmented
 * compressed panel frames. Used by FrameSender.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#include "UdpSender.h"
#include "PanelFrameHeader.h"
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
#pragma pack(push, 1)
struct UdpFragmentHeader {
	uint16_t fragIndex;
	uint16_t fragCount;
	uint32_t payloadSize;
};
#pragma pack(pop)

/**
 * @brief Splits a compressed frame into MTU-sized fragments and sends them.
 */
void UdpSender::sendPanelFragments(uint16_t panelID, uint32_t frameID, const char* data, int size, int w, int h) {
	if (socket_ < 0)
		return;

	const int mtu = 1472;
	const int headerSize = sizeof(PanelFrameHeader) + sizeof(UdpFragmentHeader);
	const int maxPayload = mtu - headerSize;

	int fragCount = (size + maxPayload - 1) / maxPayload;

	// Build WebSocket frame
	PanelFrameHeader fh{};
	fh.frameID = frameID;
	fh.panelID = panelID;
	fh.width = w;
	fh.height = h;
	fh.compSize = size;

	// Send each fragment
	for (int i = 0; i < fragCount; i++) {
		int offset = i * maxPayload;
		int chunkSize = std::min(maxPayload, size - offset);

		UdpFragmentHeader uh;
		uh.fragIndex = i;
		uh.fragCount = fragCount;
		uh.payloadSize = chunkSize;

		// Build packet buffer
		char buffer[mtu];
		memcpy(buffer, &fh, sizeof(fh));
		memcpy(buffer + sizeof(fh), &uh, sizeof(uh));
		memcpy(buffer + sizeof(fh) + sizeof(uh), data + offset, chunkSize);

		// Transmit
		sendto(socket_, buffer, headerSize + chunkSize, 0, (sockaddr*)&destAddr_, sizeof(destAddr_));
	}
}
