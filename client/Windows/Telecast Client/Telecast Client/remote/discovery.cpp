#include "discovery.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "storage/Store.h"
#include "logging/Debug.h"

#pragma comment(lib, "Ws2_32.lib")

void discoverDevices() {
	// Send discovery message to tell all available devices to make themselves known.
	if (sendto(Store::s, "<discovery>", 12, 0, (const sockaddr*)&Store::broadcast, sizeof(Store::broadcast)) == SOCKET_ERROR) {
		Debug::logError("Failure encountered while broadcasting discovery message. Error code:\n");
		Debug::logNum(WSAGetLastError());
		return;
		// TODO: Think about doing something here. But even if this fails, it isn't a breaking change right, just wait till the next time and try again.
		// TODO: Also find a way to deal with network changes and stoerungen and such in the other code.
	}

	// Listen for incoming answers.
	while (true) {						// TODO: Obviously resend the prompt once and a while because it's UDP and who knows and all that stuff.
		// Stuff.
	}
}