#pragma once

#include "storage/Store.h"
#include "debugging.h"

#include "defines.h"

sockaddr_in6 discoverers[DISCOVERER_BUFFER_LENGTH];																// Circular buffer for first-in-first-out responding to discovery requests.
unsigned int pendingDiscoverersIndex = 0;
unsigned int closedDiscoverersIndex = 0;

void incrementPending() {
	if (pendingDiscoverersIndex == DISCOVERER_BUFFER_LENGTH - 1) { pendingDiscoverersIndex = 0; return; }
	pendingDiscoverersIndex++;
}

void incrementClosed() {
	if (closedDiscoverersIndex == DISCOVERER_BUFFER_LENGTH - 1) { closedDiscoverersIndex = 0; return; }
	closedDiscoverersIndex++;
}

bool shouldDiscoveryListenRun = true;
inline void listenForDiscoveries() {
	char buffer;
	while (shouldDiscoveryListenRun) {
		// NOTE: The - 1 is necessary so that we can know if the buffer is full or empty. Gives us the "direction" of the state. You could technically do this with a boolean but there are all kinds of race conditions
		// that pop up with that. This wastes 3 bytes more memory, but is completely stable.
		if (pendingDiscoverersIndex == closedDiscoverersIndex - 1) { continue; }							// Drop packets if there is no more room in the discoverers buffer.

		int addressSize = sizeof(sockaddr_in6);
		if (recvfrom(Store::discoveryListener, &buffer, 1, 0, (sockaddr*)(discoverers + pendingDiscoverersIndex), &addressSize) == SOCKET_ERROR) {
			LOG("Couldn't receive through the discovery listener. Error code: ");
			LOGNUM(WSAGetLastError());
			return;
		}

		incrementPending();
	}
}

bool shouldDiscoveryRespondRun = true;
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
}