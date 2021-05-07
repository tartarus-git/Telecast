#include "TelecastStream.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>
#include <cstdint>

#include "storage/Store.h"
#include "debugging.h"

#include "defines.h"

#pragma comment(lib, "Ws2_32.lib")

char* TelecastStream::buffer = nullptr;

TelecastStream::TelecastStream(uint16_t dataPort, uint16_t metadataPort) : dataPort(dataPort), metadataPort(metadataPort) {
	// Allocate enough space on the heap for the display data.
	buffer = new char[SERVER_DATA_BUFFER_SIZE];

	// TODO: Is there any way to shrink this long and annoying code into something more portable or something?
	// Stream data socket setup.
	if (Store::streamDataSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP) == SOCKET_ERROR) {
		LOG("Encountered error while initializing the stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		return;
	}

	if (ioctlsocket(Store::streamDataSocket, FIONBIO, (u_long*)Store::nonblocking) == SOCKET_ERROR) {			// TODO: It seems like this function tries to modify the nonblocking thing. See if thats true.
		LOG("Encountered error while setting the stream data socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::streamDataSocket);
		return;
	}

	if (bind(Store::streamDataSocket, (const sockaddr*)&Store::local, sizeof(Store::local)) == SOCKET_ERROR) {
		LOG("Encountered error while binding stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::streamDataSocket);
		return;
	}

	// Stream metadata socket setup.
	if (Store::streamMetadataSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == SOCKET_ERROR) {
		LOG("Encountered error while initializing the stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		return;
	}

	if (ioctlsocket(Store::streamMetadataSocket, FIONBIO, (u_long*)Store::nonblocking) == SOCKET_ERROR) {			// TODO: It seems like this function tries to modify the nonblocking thing. See if thats true.
		LOG("Encountered error while setting the stream metadata socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::streamMetadataSocket);
		return;
	}

	if (bind(Store::streamMetadataSocket, (const sockaddr*)&Store::local, sizeof(Store::local)) == SOCKET_ERROR) {
		LOG("Encountered error while binding stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::streamMetadataSocket);
		return;
	}

	dataThread = std::thread(data);																				// Start the data and metadata network interfaces.
	metadataThread = std::thread(metadata);
}

TelecastStream& TelecastStream::operator=(TelecastStream&& other) noexcept {
	dataThread = std::move(other.dataThread);
	metadataThread = std::move(other.dataThread);
	other.dataPort = 0;
	return *this;
}

void TelecastStream::data() {
	while (true) {
		if (recvfrom(Store::streamDataSocket, buffer, SERVER_DATA_BUFFER_SIZE, 0, nullptr, nullptr) == SOCKET_ERROR) {
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK:
				// block stuff.
			default:
				LOG("Encountered error while receiving from the stream origin device. Error code: ");
				LOGNUM(error);
				closesocket(Store::streamDataSocket);
				return;												// TODO: Handle this gracefully and synchronize exit with the metadata socket.
			}
		}
	}
}

void TelecastStream::metadata() {
	while (true) {

	}
}

void TelecastStream::release() {
	dataThread.join();													// TODO: Make sure that this doesn't do anything bad if the thread is already joined.
	metadataThread.join();
	// There is no release method, we have to wait for the destructors to be called.
	buffer = nullptr;																								// We use this as the flag for preventing double releases.
}
TelecastStream::~TelecastStream() { if (buffer) { release(); } }