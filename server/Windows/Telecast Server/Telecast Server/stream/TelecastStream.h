#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>

#include "defines.h"

class TelecastStream {
private:
	// Addresses.
	sockaddr_in6 dataAddress;
	sockaddr_in6 metadataAddress;

	// Sockets.
	SOCKET dataSocket;																								// UDP
	SOCKET metadataSocket;																							// TCP

	// Threads.
	bool networkError = false;
	bool shouldMonitorNetworkStatus = true;
	std::thread networkStatusMonitorThread;

	bool shouldReceiveData = false;
	std::thread dataThread;
	bool shouldReceiveMetadata = true;
	std::thread metadataThread;

	// Stream data.
	char* backBuffer;																								// 2 buffers for double buffering.
	char* frontBuffer;
	unsigned int bufferIndex = 0;

	// Stream metadata.
	sockaddr_in6 currentClient;
	int currentClientSize = sizeof(sockaddr_in6);

public:
	// Stream metadata.
	SIZE size;
	
	// Status flag for when the front buffer is safe to use.
	bool isFrontBufferValid = false;

	// Status flag for when the Telecast stream is experiencing network problems.
	bool isExperiencingNetworkIssues = false;

	static void networkStatusMonitor(TelecastStream* instance);

	static void data(TelecastStream* instance);
	static void metadata(TelecastStream* instance);

	TelecastStream() = default;
	TelecastStream(u_short dataPort, u_short metadataPort);

	TelecastStream& operator=(TelecastStream&& other) noexcept;

	void halt();														// If any sort of error happens where the stream needs to be halted but the sockets can be reused later. Call this. Halts threads.
	void restart();														// This needs to be called after calling halt in order to use the resources to restart the stream systems.

	void close();
	~TelecastStream();
};