#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

class TelecastStreamClient {
	// Addresses
	sockaddr_in6 dataAddress;
	sockaddr_in6 metadataAddress;

	// Sockets
	SOCKET dataSocket;
	SOCKET metadataSocket;

	// Frame helpers.
	SIZE frameDimensions;

	bool valid;

public:
	void validate();
	void invalidate();
	bool isValid();

	bool initialize();

	TelecastStreamClient() = default;
	TelecastStreamClient(u_short dataPort, u_short metadataPort, SIZE frameDimensions);

	bool connect(sockaddr_in6 server);																									// Make the initial metadata and data connections with the server and send over the frame size. Return false if network issues happen anywhere along the way.

	bool sendFrame(const char* data);																									// Send the frame contained within the buffer pointed to by data. Returns false if network issues are encountered.
};