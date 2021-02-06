#include "discovery.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "storage/Store.h"
#include "logging/Debug.h"
#include "defines.h"

#pragma comment(lib, "Ws2_32.lib")

void discoverDevices() {
	if (listen(Store::discoveryReceiver, MAX_DEVICES) == SOCKET_ERROR) {
		Debug::logError("Failure encountered while starting to listen for anwers to discovery. Error code:\n");
		Debug::logNum(WSAGetLastError());
		return;					// TODO: Think about how to handle this.
	}

	while (true) {
loop:
		// Send discovery message to tell all available devices to make themselves known.
		if (sendto(Store::s, "<discovery>", 12, 0, (const sockaddr*)&Store::broadcast, sizeof(Store::broadcast)) == SOCKET_ERROR) {
			Debug::logError("Failure encountered while broadcasting discovery message. Error code:\n");
			Debug::logNum(WSAGetLastError());
			return;
			// TODO: Think about doing something here. But even if this fails, it isn't a breaking change right, just wait till the next time and try again.
			// TODO: Also find a way to deal with network changes and stoerungen and such in the other code.
		}

		// Listen for incoming answers.
		sockaddr_in6 sender;
		int senderLength;
		while (true) {
			// Try accepting a connection. If none are available to accept, loop again and try a few more times before resending the discovery broadcast.
			senderLength = sizeof(sender);					// TODO: This gets done even when the last accept didn't change it because of failure, change that.
			if (accept(Store::discoveryReceiver, (sockaddr*)&sender, &senderLength) == SOCKET_ERROR) {
				int error = WSAGetLastError();
				if (error == WSAEWOULDBLOCK) { continue; }					// TODO: Make this wait a second and then retry a couple of times before giving up and sending another discovery broadcast. Same for the one all the way down there.
				Debug::logError("Failure encountered while accepting an answer to discovery broadcast. Error code:\n");
				Debug::logNum(WSAGetLastError());
				return;				// TODO: Obviously think about how to handle this.
			}

			// If connection was accepted, try receiving device descriptor from it.
			Device buffer;
			int pos = 0;
			while (true) {							// TODO: Maybe convert all of the casts in your thing into c++ casts.
				int bytesReceived = recv(Store::discoveryReceiver, (char*)&buffer + pos, sizeof(buffer) - pos, 0);
				if (bytesReceived == SOCKET_ERROR) {
					int error = WSAGetLastError();
					if (error == WSAEWOULDBLOCK) { continue; }
					Debug::logError("Failure encountered while receiving device descriptor. Error code:\n");
					Debug::logNum(WSAGetLastError());
					goto loop;						// TODO: Make sure you're not doing anything stupid here. Make sure all your memory is accounted for.
				}
				pos += bytesReceived;
				if (pos == sizeof(buffer)) { break; }
			}

			// If the device isn't already in the list, add it to the list of discovered devices.
			for (int i = 0; i < Store::len_discoveredDevices; i++) {
				if (Store::discoveredDevices[i].address == buffer.address) { continue; }
			}
			Store::discoveredDevices[Store::len_discoveredDevices] = buffer;
			Store::len_discoveredDevices++;
		}
	}
}