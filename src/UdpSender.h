/**
 * @file UdpSender.h
 * @brief UDP backend for transmitting compressed panel frames.
 *
 * The UdpSender handles platform‑independent UDP socket creation and sends
 * fragmented LZ4‑compressed panel frames to a remote receiver.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#pragma once
#include "RawPanelFrame.h"
#include <cstdint>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

/**
 * @brief Handles UDP socket creation and transmission of fragmented panel data.
 *
 * Frames are split into MTU‑sized fragments, each prefixed with a header
 * containing metadata required for reassembly on the receiver side.
 */
class UdpSender {
  public:
	UdpSender() = default;
	~UdpSender();

	bool init(const char* ip, uint16_t port);
	void close();

	void sendPanelFragments(uint16_t panelID, uint32_t frameID, const char* compData, int compSize, int w, int h);

  private:
	int socket_ = -1;
	sockaddr_in destAddr_{};
};
