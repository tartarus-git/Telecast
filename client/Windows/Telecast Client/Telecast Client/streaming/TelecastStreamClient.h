#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#define IPV6_MIN_MTU 1280

class TelecastStreamClient {
	// Addresses
	sockaddr_in6 dataAddress;
	sockaddr_in6 metadataAddress;

	// Sockets
	SOCKET dataSocket;
	SOCKET metadataSocket;

	// Frame helpers.
	SIZE frameDimensions;
	uint64_t frameSize;																											// TODO: Make sure this is necessary for future proofing. Are we close to needing a 64-bit integer in this day and age for video transmission?

	// Networking.
	unsigned int packetFrameDataLength;							// TODO: A short might be fine for this one. NOTE: This one is the packet length minus the amount of data responsible for the id of the packet.

	bool valid;

public:
	void validate();
	void invalidate();
	bool isValid();

	bool initialize();

	TelecastStreamClient() = default;
	TelecastStreamClient(u_short dataPort, u_short metadataPort, SIZE frameDimensions);

	bool connectToServer(const sockaddr_in6* server);																							// Make the initial metadata and data connections with the server and send over the frame size. Return false if network issues happen anywhere along the way.

	bool sendFrame(const char* data);																									// Send the frame contained within the buffer pointed to by data. Returns false if network issues are encountered.
};