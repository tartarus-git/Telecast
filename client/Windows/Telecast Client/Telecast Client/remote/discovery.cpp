#include "discovery.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "storage/Store.h"
#include "logging/Debug.h"
#include "defines.h"

#pragma comment(lib, "Ws2_32.lib")

void discoverDevices() {
	// Reset list of discovered devices.
	Store::len_discoveredDevices = 0;

	if (listen(Store::discoveryReceiver, MAX_DEVICES) == SOCKET_ERROR) {
		Debug::logError("Failure encountered while starting to listen for anwers to discovery. Error code:\n");
		Debug::logNum(WSAGetLastError());
		return;					// TODO: Think about how to handle this.
	}

	while (Store::doDiscovery) {
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
		bool retry = false;
		while (Store::doDiscovery) {
loop:
			// Try accepting a connection. If none are available to accept, wait for a bit and try again. If nothing changes, resend discovery broadcast.
			senderLength = sizeof(sender);					// TODO: This gets done even when the last accept didn't change it because of failure, change that.
			if (accept(Store::discoveryReceiver, (sockaddr*)&sender, &senderLength) == SOCKET_ERROR) {
				int error = WSAGetLastError();
				if (error == WSAEWOULDBLOCK) {
					if (retry) { break; }			// TODO: Is there any better way to do this. Writing it two times might not even be right this time because of the if statement and stuff.
					retry = true;
					Sleep(ACCEPT_SLEEP);
					continue;
				}
				Debug::logError("Failure encountered while accepting an answer to discovery broadcast. Error code:\n");
				Debug::logNum(WSAGetLastError());
				return;				// TODO: Obviously think about how to handle this.
			}

			// If connection was accepted, try receiving device descriptor from it.
			Device buffer;
			int pos = 0;
			// TODO: This has to be true instead of isDescoveryAlive right? I mean, if this stops half way through then you have an invalid device or something.
			// Is there a way to get away with putting it here? Something to make more efficient?
			while (true) {							// TODO: Maybe convert all of the casts in your thing into c++ casts.
				int bytesReceived = recv(Store::discoveryReceiver, (char*)&buffer + pos, sizeof(buffer) - pos, 0);
				if (bytesReceived == SOCKET_ERROR) {
					int error = WSAGetLastError();
					if (error == WSAEWOULDBLOCK) { continue; }
					Debug::logError("Failure encountered while receiving device descriptor. Error code:\n");
					Debug::logNum(WSAGetLastError());
					goto loop;						// TODO: Make sure you're not doing anything stupid here. Make sure all your memory is accounted for.
					// TODO: Notes for this goto:
					//		- It should goto loop if the device disconnects.
					//		- It should return if there is something wrong with the clientside that won't fix itself.
					//		Your gonna have to handle some errors here instead of just leaving.
				}
				pos += bytesReceived;
				if (pos == sizeof(buffer)) { break; }
			}

			// If the device isn't already in the list, add it to the list of discovered devices.
			for (int i = 0; i < Store::len_discoveredDevices; i++) {
				if (Store::discoveredDevices[i].address == buffer.address) { goto loop; }
			}
			Store::discoveredDevices[Store::len_discoveredDevices] = buffer;
			Store::len_discoveredDevices++;
		}
	}
}