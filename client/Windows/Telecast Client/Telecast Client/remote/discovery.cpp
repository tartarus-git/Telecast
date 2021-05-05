#include "discovery.h"

#include <winsock2.h>																				// Winsock2 includes.
#include <ws2tcpip.h>

#include "storage/Store.h"																			// Global variables that we use all over the place, mainly here.
#include "logging/Debug.h"																			// Debugging helpers.

#include "defines.h"																				// Useful global defines.

#pragma comment(lib, "Ws2_32.lib")																	// Statically link to the winsock2 library. TODO: Make sure what I said is right.

bool discoverDevices() {																			// Return false if something went wrong while discovering devices. True if everything is fine.
	Store::len_discoveredDevices = 0;																// Reset list of discovered devices, we're going to fill it up again.

	if (listen(Store::discoveryListener, MAX_DEVICES) == SOCKET_ERROR) {
		Debug::logError("Failure encountered while starting to listen for answers to discovery. Error code: ");
		Debug::logNum(WSAGetLastError());
		return false;
	}

	while (Store::doDiscovery) {																											// doDiscovery flags tells this thread when to quit.
		if (sendto(Store::s, "<discovery>", 12, 0, (const sockaddr*)&Store::broadcast, sizeof(Store::broadcast)) == SOCKET_ERROR) {			// Broadcast discovery message to make this device known.
			Debug::logError("Failure encountered while broadcasting discovery message. Error code: ");
			Debug::logNum(WSAGetLastError());
			return false;
		}

		// Listen for incoming answers.
		sockaddr_in6 sender;				// TODO: What kind of error does accept throw if the remote thing doesn't support IPv6? Is that even possible if we're only giving him IPv6 options?
		int senderLength;
		while (Store::doDiscovery) {																						// Another doDiscovery check because inner loop is going to run by itself a little.
loop:
			senderLength = sizeof(sender);					// TODO: This gets done even when the last accept didn't change it because of failure, change that. Figure out if the function even changes this value.
			SOCKET discoveryReceiver = accept(Store::discoveryListener, (sockaddr*)&sender, &senderLength);					// Try accepting a discovery response, if nothing happens for two tries, send again.
			if (discoveryReceiver == SOCKET_ERROR) {
				int error = WSAGetLastError();
				if (error == WSAEWOULDBLOCK) {
					Sleep(ACCEPT_SLEEP);

					discoveryReceiver = accept(Store::discoveryListener, (sockaddr*)&sender, &senderLength);				// Second try at accepting some response data. If fails, break out of inner loop.
					if (discoveryReceiver == SOCKET_ERROR) {
						error = WSAGetLastError();
						if (error == WSAEWOULDBLOCK) { break; }
					}

				}
				Debug::logError("Failure encountered while accepting an answer to discovery broadcast. Error code: ");		// If the socket error is something else (potentially more serious), then throw error.
				Debug::logNum(WSAGetLastError());
				return false;
			}

			// TODO: This isn't necessary until we start messing around with encryption, so just copy the sender address for now.
			// If connection was accepted, try receiving device descriptor from it.
			/*Device buffer;
			int pos = 0;
			while (Store::doDiscovery) {																					// If this stops badly, invalid device will be in list. User has to account for that.
				int bytesReceived = recv(discoveryReceiver, (char*)&buffer + pos, sizeof(buffer) - pos, 0);					// Try to receive from the responder.
				if (bytesReceived == SOCKET_ERROR) {
					int error = WSAGetLastError();
					// TODO: Is there a built-in timeout or something, should we make our own timeout for this response receiving thing? It doesn't really freeze up the thread, which is good, but who knows.
					switch (error) {
					case WSAEWOULDBLOCK: continue;																			// If it would block, just continue looping and wait for a response.
					case DISCONNECT:																						// If the responder disconnects, just start accepting a new responder.
						closesocket(discoveryReceiver);
						Debug::logError("Discovery responder disconnected, accepting a new responder...");
						goto loop;
					case NETWORK_DISCONNECT:																				// If the network disconnects, throw error and stop discovering.
						closesocket(discoveryReceiver);
						Debug::logError("Network disconnected while discovering, halting discovery...");
						return false;
					default:
						closesocket(discoveryReceiver);																		// If error doesn't match template, throw generic error and halt discovery.
						Debug::logError("Error encountered while receiving from the discovery responder. Error code: ");
						Debug::logNum(error);
						return false;
					}
				}
				pos += bytesReceived;
				if (pos == sizeof(buffer)) { break; }

				getpeername()
			}*/

			closesocket(discoveryReceiver);																					// Close connection with the responder, he did his job and that's that.

			// If the device isn't already in the list, add	it to the list of discovered devices.
			Device responder = { sender };													// TODO: Make this not necessary, that means adding an operator overload to the sockaddr_in6 from this scope.
																																// Don't edit anything in any system source code plz.
			for (int i = 0; i < Store::len_discoveredDevices; i++) {
				if (Store::discoveredDevices[i] == responder) { continue; }													// If this device already exists in the list, just look for a new one.
			}
			Store::discoveredDevices[Store::len_discoveredDevices] = responder;												// If it doesn't, add it.
			Store::len_discoveredDevices++;
		}
	}
}