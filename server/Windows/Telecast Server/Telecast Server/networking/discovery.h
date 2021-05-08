#pragma once

#include "debugging.h"																									// Defines useful macros for debugging.

#include "defines.h"																									// Useful defines.

inline void setupDiscoveryListenerSocket() {

}

inline void setupDiscoveryResponderSocket() {

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

		int addressSize = sizeof(sockaddr_in6);
		if (recvfrom(discoveryListener, &buffer, 1, 0,																	// Try to receive a UDP discovery broadcast.
			(sockaddr*)(discoverers + pendingDiscoverersIndex), &addressSize) == SOCKET_ERROR) {
			LOG("Couldn't receive through the discovery listener. Error code: ");
			LOGNUM(WSAGetLastError());
			return;
		}

		incrementPending();
	}
}

bool shouldDiscoveryRespondRun = true;																					// Flag for telling the discovery responder when to stop. Is changed by the main thread.
inline void respondToDiscoveries() {
	while (shouldDiscoveryRespondRun) {
		// NOTE: - 1 is not necessary here.
		if (closedDiscoverersIndex == pendingDiscoverersIndex) { continue; }								// Don't do anything if there is no more room in discoverers buffer.

		// TODO: Make sure you understand exactly what is happening with the sockaddr size thing. Can we leave the param for that out here since it technically is optional?
		if (connect(Store::discoveryResponder, (const sockaddr*)(discoverers + closedDiscoverersIndex), sizeof(sockaddr_in6)) == SOCKET_ERROR) {
			LOG("Couldn't connect to discoverer. Error code: ");
			LOGNUM(WSAGetLastError());												// TODO: Look through this error handling and other error handling and see which ones need switch cases for behaviour.
			continue;
		}

		if (send(Store::discoveryResponder, Store::discoveryResponse, sizeof(Store::discoveryResponse), 0) == SOCKET_ERROR) {
			LOG("Couldn't send the discovery response to discoverer. Error code: ");
			LOGNUM(WSAGetLastError());
		}

		shutdown(Store::discoveryResponder, SD_SEND);														// Shutdown the transmission (only send is necessary because we don't receive) and increment the close index.
		incrementClosed();
	}

	closesocket(Store::discoveryResponder);
}