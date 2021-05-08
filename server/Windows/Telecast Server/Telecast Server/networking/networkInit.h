#pragma once

#include "storage/Store.h"
#include "debugging.h"

#include "defines.h"

inline bool initNetwork() {
	if (WSAStartup(MAKEWORD(2, 2), &Store::wsaData) == SOCKET_ERROR) {											// Initialize WinSock2. // TODO: Find out what this function actually does. Like why is it needed?
		LOG("Failed to initialize WinSock2. Error code: ");
		LOGNUM(WSAGetLastError());
		return false;
	}

	if (Store::discoveryListener = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP) == INVALID_SOCKET) {				// Initialize the discovery listener socket. This will listen for discovery broacasts.
		LOG("Failed to discovery listener socket. Error code: ");
		LOGNUM(WSAGetLastError());
		return false;
	}

	u_long nonblocking = true;
	if (ioctlsocket(Store::discoveryListener, FIONBIO, &nonblocking) == SOCKET_ERROR) {							// Set the discovery listener socket to non-blocking so that network logic works.
		LOG("Failed to set the discovery listener socket so non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);																	// Free resources of the listener socket before returning.
		return false;
	}

	sockaddr_in6 local = { };																					// Set up the local address for this machine.
	local.sin6_family = AF_INET6;
	local.sin6_addr = in6addr_any;											// TODO: Make sure you understand exactly what this does.
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then? What the hell is the scope ID.
	local.sin6_port = htons(SERVER_DISCOVERY_PORT);																// Convert server port from host byte order to network byte order (big-endian).

	if (bind(Store::discoveryListener, (const sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {				// Bind the discovery listener socket.
		LOG("Failed to bind the discovery listener socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);
		return false;
	}

	// No need to join any sort of multicast group. The discovery protocol uses the all nodes multicast group, which all nodes are already a part of.

	if (Store::discoveryResponder = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == INVALID_SOCKET) {				// Initialize the discovery responder socket. This will send TCP responses to UDP multicast discoveries.
		LOG("Failed to discovery responder socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);
		return false;
	}

	if (ioctlsocket(Store::discoveryListener, FIONBIO, &nonblocking) == SOCKET_ERROR) {
		LOG("Failed to set the discovery responder socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);
		closesocket(Store::discoveryResponder);
		return false;
	}

	if (bind(Store::discoveryResponder, (const sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {				// Bind the discovery responder socket.
		LOG("Failed to bind the discovery responder socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);
		closesocket(Store::discoveryResponder);
		return false;
	}
}