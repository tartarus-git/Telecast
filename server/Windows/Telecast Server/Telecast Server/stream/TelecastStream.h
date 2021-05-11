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
	SOCKET dataSocket;																										// UDP
	SOCKET metadataListenerSocket;																							// TCP

	// Threads.
	bool networkError;
	bool shouldMonitorNetworkStatus;
	std::thread networkStatusMonitorThread;

	bool shouldReceiveData = false;
	std::thread dataThread;
	bool shouldReceiveMetadata = true;
	std::thread metadataThread;

	// Stream data.
	char* backBuffer = nullptr;																								// 2 buffers for double buffering.
	char* frontBuffer;
	unsigned int bufferIndex = 0;

	// Stream metadata.
	sockaddr_in6 currentClient;
	int currentClientSize = sizeof(sockaddr_in6);

	void invalidate();

public:
	// Stream metadata.
	SIZE size;
	
	// Status flag for when the front buffer is safe to use.
	bool isFrontBufferValid = false;

	// Status flag for when the Telecast stream is experiencing network problems.
	bool isExperiencingNetworkIssues = false;

	bool isValid();

	void halt();														// If any sort of error happens where the stream needs to be halted but the sockets can be reused later. Call this. Halts data and metadata threads. Network monitor thread is left intact.

	static void networkStatusMonitor(TelecastStream* instance);

	static void data(TelecastStream* instance);
	static void metadata(TelecastStream* instance);

	void start();																											// Starts all the necessary systems in order to function.

	TelecastStream() = default;
	TelecastStream(u_short dataPort, u_short metadataPort);

	TelecastStream& operator=(TelecastStream&& other) noexcept;

	void close();
	~TelecastStream();
};