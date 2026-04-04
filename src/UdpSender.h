/**
 * @file UdpSender.h
 * @brief UDP transmission backend for sending compressed panel frames.
 *
 * Provides platform‑independent UDP socket handling for Windows and POSIX.
 * Part of the Panelcast plugin for X‑Plane.
 * (c) 2025 Peter — All rights reserved.
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
 */
class UdpSender {
  public:
	UdpSender() = default;
	~UdpSender();

	bool init(const char* ip, uint16_t port);
	void close();

	void sendPanelFragments(uint16_t panelID, uint32_t frameID, const char* compData, int compSize, int w, int h);

  private:
	int socket = -1;
	sockaddr_in destAddr{};
};
