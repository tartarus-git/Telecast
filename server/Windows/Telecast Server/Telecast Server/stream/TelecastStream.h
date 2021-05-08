#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>
#include <cstdint>

#include "defines.h"

class TelecastStream {
private:
	// Sockets.
	SOCKET dataSocket;																								// UDP
	SOCKET metadataSocket;																							// TCP

	// Threads.
	bool shouldReceiveData = false;
	std::thread dataThread;
	bool shouldReceiveMetadata = true;
	std::thread metadataThread;

	// Stream data.
	char* buffer;
	unsigned int bufferIndex = 0;

	// Stream metadata.
	sockaddr_in6 currentClient;
	int currentClientSize = sizeof(sockaddr_in6);

public:
	// Stream metadata.
	SIZE size;

	static void data(TelecastStream* instance);
	static void metadata(TelecastStream* instance);

	TelecastStream(uint16_t dataPort, uint16_t metadataPort);

	TelecastStream& operator=(TelecastStream&& other) noexcept;

	void release();
	~TelecastStream();
};