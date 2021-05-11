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

public:
	void initialize();

	TelecastStreamClient() = default;
	TelecastStreamClient(u_short dataPort, u_short metadataPort, SIZE frameDimensions);

	void connect(sockaddr_in6 server);

	void sendFrame(const char* data);
};