#pragma once

#include "storage/Store.h"
#include "debugging.h"

#include "defines.h"

sockaddr_in6 discoverers[DISCOVERER_BUFFER_LENGTH];
unsigned int discoverersIndex = 0;

bool shouldDiscoveryListenRun = true;
inline void listenForDiscoveries() {
	char buffer;
	while (shouldDiscoveryListenRun) {
		if (discoverersIndex == DISCOVERER_BUFFER_LENGTH) { continue; }																			// Drop packets if there is no more room in the discoverers buffer.

		int addressSize = sizeof(sockaddr_in6);
		if (recvfrom(Store::discoveryListener, &buffer, 1, 0, (sockaddr*)(discoverers + discoverersIndex), &addressSize) == SOCKET_ERROR) {
			LOG("Couldn't receive through the discovery listener. Error code: ");
			LOGNUM(WSAGetLastError());
			return;
		}
		discoverersIndex++;
	}
}

bool shouldDiscoveryRespondRun = true;
inline void respondToDiscoveries() {
	while (shouldDiscoveryRespondRun) {
		if (discoverersIndex == 0) { continue; }																								// Don't do anything if there is no more room in discoverers buffer.

		// TODO: Make sure you understand exactly what is happening with the sockaddr size thing. Can we leave the param for that out here since it technically is optional?
		if (connect(Store::discoveryResponder, (const sockaddr*)(discoverers + discoverersIndex), sizeof(sockaddr_in6)) == SOCKET_ERROR) {
			LOG("Couldn't connect to discoverer. Error code: ");
			LOGNUM(WSAGetLastError());												// TODO: Look through this error handling and other error handling and see which ones need switch cases for behaviour.
			continue;
		}

		if (send(Store::discoveryResponder, Store::discoveryResponse, sizeof(Store::discoveryResponse), 0) == SOCKET_ERROR) {
			LOG("Couldn't send the discovery response to discoverer. Error code: ");
			LOGNUM(WSAGetLastError());
			shutdown(Store::discoveryResponder, SD_SEND);											// TODO: Is this necessary here, I guess it depends on the output of a switch on the error that we get. Do that.
			continue;
		}
	}
}