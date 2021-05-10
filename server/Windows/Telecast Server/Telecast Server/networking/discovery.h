#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include "debugging.h"																														// Defines useful macros for debugging.

#include "defines.h"																														// Useful defines.

#pragma comment(lib, "Ws2_32.lib")

static const char* discoveryResponse;																					// TODO: Read this from a setup file eventually.

sockaddr_in6 discoveryAddress;
inline void initializeLocalDiscoveryAddress() {
	discoveryAddress = { };																													// Set up the local address for this machine.
	discoveryAddress.sin6_family = AF_INET6;																								// Set the address family to IPv6. This means that the system only accepts IPv6 addresses.
	discoveryAddress.sin6_addr = in6addr_any;																								// Just set the local address to 0.0.0.0, which selects the default address.
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then? What the hell is the scope ID.
	discoveryAddress.sin6_port = htons(SERVER_DISCOVERY_PORT);																				// Convert server discovery port from host byte order to network byte order (big-endian).
}

SOCKET discoveryListener;
inline void setupDiscoveryListenerSocket() {
	if (discoveryListener = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP) == INVALID_SOCKET) {													// Initialize the discovery listener socket. This will listen for discovery broacasts.
		LOG("Failed to initialize discovery listener socket. Error code: ");								// TODO: Actually handle the errors that can happen here.
		LOGNUM(WSAGetLastError());
		ASSERT(false);
	}

	// TODO: Seems like this call wants to set the nonblocking variable. Why is that. Read the docs.
	if (ioctlsocket(discoveryListener, FIONBIO, (u_long*)&Store::nonblocking) == SOCKET_ERROR) {											// Set the discovery listener socket to non-blocking so that network logic works.
		LOG("Failed to set the discovery listener socket so non-blocking. Error code: ");					// TODO: Do actual error handling here eventually.
		LOGNUM(WSAGetLastError());
		closesocket(discoveryListener);																										// Free resources of the listener socket before asserting.
		ASSERT(false);
	}

	if (bind(discoveryListener, (const sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) == SOCKET_ERROR) {							// Bind the discovery listener socket.
		LOG("Failed to bind the discovery listener socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(discoveryListener);
		ASSERT(false);																						// TODO: Do actual error handling for this eventually.
	}
}

SOCKET discoveryResponder;
inline void setupDiscoveryResponderSocket() {
	if (discoveryResponder = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == INVALID_SOCKET) {												// Initialize the discovery responder socket. This will send TCP responses to UDP multicast discoveries.
		LOG("Failed to initialize discovery responder socket. Error code: ");
		LOGNUM(WSAGetLastError());
		ASSERT(false);																						// TODO: Do actual error handling for this eventually.
	}

	// TODO: Figure out why this function could edit the nonblocking thing.
	if (ioctlsocket(discoveryResponder, FIONBIO, (u_long*)&Store::nonblocking) == SOCKET_ERROR) {											// Set the discovery responder socket to nonblocking.
		LOG("Failed to set the discovery responder socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(discoveryResponder);
		ASSERT(false);																						// TODO: Do actual error handling for this eventually.
	}

	if (bind(discoveryResponder, (const sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) == SOCKET_ERROR) {							// Bind the discovery responder socket.
		LOG("Failed to bind the discovery responder socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(discoveryResponder);
		ASSERT(false);
	}
}

sockaddr_in6 discoverers[DISCOVERER_BUFFER_LENGTH];																		// Circular buffer for first-in-first-out responding to discovery requests.
unsigned int pendingDiscoverersIndex = 0;
unsigned int closedDiscoverersIndex = 0;

void incrementPending() {																								// Increment the pending index in a circular fashion.
	if (pendingDiscoverersIndex == DISCOVERER_BUFFER_LENGTH - 1) { pendingDiscoverersIndex = 0; return; }
	pendingDiscoverersIndex++;
}

void incrementClosed() {																								// Increment the closed index in a circular fashion.
	if (closedDiscoverersIndex == DISCOVERER_BUFFER_LENGTH - 1) { closedDiscoverersIndex = 0; return; }
	closedDiscoverersIndex++;
}

bool shouldDiscoveryListenRun = true;																					// Flag for telling the discovery listener when to stop. Is changed by main thread.
inline void listenForDiscoveries() {																					// Listen for incoming UDP discovery broadcasts.
	setupDiscoveryListenerSocket();																						// Set up the UDP discovery listener socket which will be used here.

	char buffer;																										// 1-byte long buffer for storing the minimum amount of data required for transmission.
	while (shouldDiscoveryListenRun) {																					// Loop as long as flag is set.
		if (pendingDiscoverersIndex == closedDiscoverersIndex - 1) { continue; }										// Drop transmissions if there is no more room in the discoverers buffer.
		// NOTE: The - 1 at the end is no mistake, it is needed for the circular buffer. I tried using a simple boolean flag, but that could cause race conditions. This wastes 3 more bytes than a boolean, but it
		// works better.

	receive:
		int addressSize = sizeof(sockaddr_in6);
		if (recvfrom(discoveryListener, &buffer, 1, 0,																	// Try to receive a UDP discovery broadcast.
			(sockaddr*)(discoverers + pendingDiscoverersIndex), &addressSize) == SOCKET_ERROR) {
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: case WSAEMSGSIZE: goto receive;														// If nothing is received or msg is too long, try receiving again. Skip the buffer check.
			case WSAENETDOWN:																							// If the network is down, unset run flag and exit this thread. Network monitor will handle it.
				shouldDiscoveryListenRun = false;
				return;																																						// Make sure to return if the network fails, because or else the socket gets closed at the end of this function.
			case WSAENETRESET:
				shouldDiscoveryListenRun = false;																		// TODO: Find out more about this error. This might not be the best way to handle this one.
				return;																																						// Make sure to return if the network fails, because or else the socket gets closed at the end of this function.
			default:
				LOG("Unhandled error encountered while trying to receive discovery broadcasts. Error code: ");
				LOGNUM(error);
				ASSERT(false);
			}
		}

		incrementPending();
	}
}

bool shouldDiscoveryRespondRun = true;																					// Flag for telling the discovery responder when to stop. Is changed by the main thread.
inline void respondToDiscoveries() {																					// Respond to UDP discovery broadcasts with TCP messages.
	setupDiscoveryResponderSocket();																					// Set up the TCP socket used for responding to the broadcasts.

	while (shouldDiscoveryRespondRun) {																					// Loop as long as flag is set.
		// NOTE: - 1 is not necessary here.
		if (closedDiscoverersIndex == pendingDiscoverersIndex) { continue; }											// Don't do anything if there are no pending discoverers to respond to.

	connect:
		// TODO: Make sure you understand exactly what is happening with the sockaddr size thing. Can we leave the param for that out here since it technically is optional?
		if (connect(discoveryResponder, (const sockaddr*)(discoverers + closedDiscoverersIndex), sizeof(sockaddr_in6)) == SOCKET_ERROR) {									// Connect to the source of the broadcast and send a response through TCP.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: goto connect;
			case WSAETIMEDOUT: case WSAEHOSTUNREACH: case WSAECONNREFUSED:
				incrementClosed();
				continue;
			case WSAENETDOWN:
				shouldDiscoveryRespondRun = false;
				return;																																						// Make sure to return if the network fails, because or else the socket gets closed at the end of this function.
			default:
				LOG("Unhandled error encountered while trying to connect to discoverer. Error code: ");
				LOGNUM(error);
				ASSERT(false);
			}
		}

	send:
		if (send(discoveryResponder, discoveryResponse, sizeof(discoveryResponse), 0) == SOCKET_ERROR) {
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: goto send;
			case WSAETIMEDOUT: case WSAECONNABORTED: case WSAEHOSTUNREACH: 
				incrementClosed();
				continue;
			case WSAENETDOWN: case WSAENETRESET:
				shouldDiscoveryRespondRun = false;
				return;																																						// Make sure to return if the network fails, because or else the socket gets closed at the end of this function.
			default:
				LOG("Unhandled error encountered while trying to send a response to discoverer. Error code: ");
				LOGNUM(error);
				ASSERT(false);
			}
		}

		shutdown(discoveryResponder, SD_SEND);																																// Shutdown the transmission (only send is necessary because we don't receive) and increment the close index.
		incrementClosed();
	}

	closesocket(discoveryResponder);
}