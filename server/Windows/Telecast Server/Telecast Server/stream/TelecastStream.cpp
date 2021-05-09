#include "TelecastStream.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>

#include "storage/Store.h"
#include "debugging.h"

#include "defines.h"

#pragma comment(lib, "Ws2_32.lib")

bool operator==(sockaddr_in6 left, sockaddr_in6 right) {							// TODO: This looks insane, there must be a better way, google how to do this one.
	if (left.sin6_addr.u.Byte == right.sin6_addr.u.Byte && left.sin6_addr.u.Word == right.sin6_addr.u.Word) {
		if (left.sin6_family == right.sin6_family) {
			if (left.sin6_port == right.sin6_port) {
				if (left.sin6_flowinfo == right.sin6_flowinfo) {
					if (left.sin6_scope_id == right.sin6_scope_id) {
						if (left.sin6_scope_struct.Level == right.sin6_scope_struct.Level) {
							if (left.sin6_scope_struct.Value == right.sin6_scope_struct.Value) {
								if (left.sin6_scope_struct.Zone == right.sin6_scope_struct.Zone) {
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}
bool operator!=(sockaddr_in6 left, sockaddr_in6 right) { return !operator==(left, right); }

TelecastStream::TelecastStream(u_short dataPort, u_short metadataPort) {
	// Initialize addresses for the data and metadata sockets.
	dataAddress = { };
	dataAddress.sin6_family = AF_INET6;
	dataAddress.sin6_addr = in6addr_any;
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then? What is the scope id?
	dataAddress.sin6_port = htons(dataPort);														// Converts from host byte order to network byte order (big-endian).

	metadataAddress = { };
	metadataAddress.sin6_family = AF_INET6;
	metadataAddress.sin6_addr = in6addr_any;
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then?
	metadataAddress.sin6_port = htons(metadataPort);

	// Allocate enough space on the heap for the display data.
	backBuffer = new char[SERVER_DATA_BUFFER_SIZE];																												// Allocate 2 same-sized buffers so that we can double buffer this.
	frontBuffer = new char[SERVER_DATA_BUFFER_SIZE];

	// TODO: Is there any way to shrink this long and annoying code into something more portable or something?

	// Stream data socket setup.
	if ((dataSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {
		LOG("Encountered error while initializing the stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		ASSERT(false);
	}

	u_long nonblocking = true;
	if (ioctlsocket(dataSocket, FIONBIO, &nonblocking) == SOCKET_ERROR) {																		// Even though this looks weird, from what I could find in the docs, this function does not try to modify the nonblocking variable.
		LOG("Encountered error while setting the stream data socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(dataSocket);
		ASSERT(false);
	}

	if (bind(dataSocket, (const sockaddr*)&dataAddress, sizeof(dataAddress)) == SOCKET_ERROR) {
		LOG("Encountered error while binding stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(dataSocket);
		ASSERT(true);
	}

	// Stream metadata socket setup.
	if (metadataSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == SOCKET_ERROR) {
		LOG("Encountered error while initializing the stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		ASSERT(true);
	}

	if (ioctlsocket(metadataSocket, FIONBIO, (u_long*)Store::nonblocking) == SOCKET_ERROR) {
		LOG("Encountered error while setting the stream metadata socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(metadataSocket);
		ASSERT(true);
	}

	if (bind(metadataSocket, (const sockaddr*)&metadataAddress, sizeof(metadataAddress)) == SOCKET_ERROR) {
		LOG("Encountered error while binding stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(metadataSocket);
		ASSERT(true);
	}

	dataThread = std::thread(data, this);																														// Start the data and metadata network interfaces.
	metadataThread = std::thread(metadata, this);
}

TelecastStream& TelecastStream::operator=(TelecastStream&& other) noexcept {																			// TODO: This is a lot of stuff to copy, consider making an initialize function so that we don't have to copy everything from the temporary.
	dataAddress = other.dataAddress;
	metadataAddress = other.metadataAddress;

	networkStatusMonitorThread = std::move(other.networkStatusMonitorThread);

	dataThread = std::move(other.dataThread);
	metadataThread = std::move(other.dataThread);

	frontBuffer = other.frontBuffer;
	backBuffer = other.backBuffer;

	currentClient = other.currentClient;

	size = other.size;

	isFrontBufferValid = other.isFrontBufferValid;

	isExperiencingNetworkIssues = other.isExperiencingNetworkIssues;

	other.backBuffer = nullptr;
	return *this;
}

void TelecastStream::networkStatusMonitor(TelecastStream* instance) {
	while (instance->shouldMonitorNetworkStatus) {
		if (instance->networkError) {																					// If the networkError flag is set, shutdown data and metadata threads.
			instance->shouldReceiveMetadata = false;
			instance->metadataThread.join();
			instance->shouldReceiveData = false;
			instance->dataThread.join();
			instance->isExperiencingNetworkIssues = true;																// Set the public network issues flag to true so the main network manager can see it.
			return;
		}
	}
}

void TelecastStream::data(TelecastStream* instance) {
	sockaddr_in6 dataClient;								// TODO: Look into IP-spoofing, how easy is it to get past this simple security system.
	int dataClientSize = sizeof(dataClient);

	while (instance->shouldReceiveData) {
		int bytesReceived;
		if ((bytesReceived = recvfrom(instance->dataSocket, instance->backBuffer, SERVER_DATA_BUFFER_SIZE - instance->bufferIndex, 0, (sockaddr*)&dataClient, &dataClientSize)) == SOCKET_ERROR) {					// Try to receive from the UDP data port.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: case WSAEMSGSIZE: continue;																																						// If there is nothing to receive, just keep trying.
			case WSAENETDOWN: case WSAENETRESET: case WSAETIMEDOUT:
				instance->networkError = true;
				return;
			default:
				LOGERROR("Unhandled error message while receiving data from stream source.", error);																												// Break if the winsock call throws any sort of unhandled error.
			}
		}

		// If nothing goes wrong, make sure that the data client is the same client as the metadata client.
		// The following has to be checked every time because of UDP socket.
		if (dataClient == instance->currentClient) {															// TODO: Do I have to compare lengths too? Does that make any sense?
			instance->bufferIndex += bytesReceived;

			if (instance->bufferIndex == SERVER_DATA_BUFFER_SIZE) {												// If the buffer is complete. Do a buffer swap for double buffering and start drawing on the other one.
				while (instance->isFrontBufferValid) { }														// Only swap the buffers if the drawing code if done with the front buffer.
				
				char* temp = instance->frontBuffer;
				instance->frontBuffer = instance->backBuffer;
				instance->backBuffer = temp;

				instance->isFrontBufferValid = true;															// Set flag so that drawing code knows when it's safe to draw the frame.

				instance->bufferIndex = 0;																		// Set the index so that a new image is written on top of the old one in the back buffer.
			}
		}
	}

	// If we shouldn't receive data anymore, shutdown the socket. Don't close yet because we might start up again.
	shutdown(instance->dataSocket, SD_RECEIVE);																	// Shutdown the receive side of the socket since we're not sending anything.
}

void TelecastStream::metadata(TelecastStream* instance) {
	if (listen(instance->metadataSocket, 1) == SOCKET_ERROR) {										// TODO: Find out if there is a time limit for how long a device can stay in the backlog here.
		LOG("Encountered error while starting to listen on the metadataSocket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(instance->metadataSocket);
		return;
	}
	while (instance->shouldReceiveMetadata) {
		// Save the current client for security reasons. We can't be streaming data from someone other than who we're getting metadata from.
		if (accept(instance->metadataSocket, (sockaddr*)&instance->currentClient, &instance->currentClientSize) == SOCKET_ERROR) {			// Be ready to accept a connection to the metadata socket.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK:
				continue;																							// If nothing to accept, keep trying.
			default:
				LOG("Encountered error while accepting on the metadataSocket. Error code: ");
				LOGNUM(WSAGetLastError());
				closesocket(instance->metadataSocket);
				return;													// TODO: Obviously make more cases here and handle everything perfectly.
			}
		}

		// Once were here, something has been accepted.
		if (recv(instance->metadataSocket, (char*)&instance->size, sizeof(instance->size), 0) == SOCKET_ERROR) {
			LOG("Encountered error while receiving stream size. Error code: ");
			LOGNUM(WSAGetLastError());
			return;
		}

		// Once we receive the size, we can open up the stream data port for business.
		instance->shouldReceiveData = true;

		while (instance->shouldReceiveMetadata) {
			// Detect if closed.
			// If closed, then:
			continue;
		}
		shutdown(instance->metadataSocket, SD_BOTH);
		return;
	}
}

void TelecastStream::halt() {																							// Shutdown the data and metadata thread.
	shouldReceiveMetadata = false;
	metadataThread.join();																								// There is no release method, we have to wait for the destructors to be called.
	shouldReceiveData = false;
	dataThread.join();													// TODO: Make sure that this doesn't do anything bad if the thread is already joined.
}

void TelecastStream::restart() {																						// Restart necessary systems in order to get stream back up after usage of halt.
	if (bind(dataSocket, (const sockaddr*)&dataAddress, sizeof(metadataAddress)) == SOCKET_ERROR) {						// Bind the data socket.
		LOG("Encountered error while binding stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		ASSERT(true);
	}
	if (bind(metadataSocket, (const sockaddr*)&metadataAddress, sizeof(metadataAddress)) == SOCKET_ERROR) {				// Bind the metadata socket.
		LOG("Encountered error while binding stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		ASSERT(true);
	}

	shouldReceiveMetadata = true;																						// Restart data and metadata threads.
	metadataThread = std::thread(metadata, this);
	shouldReceiveData = true;
	dataThread = std::thread(data, this);
}

void TelecastStream::close() {
	halt();																												// Halt threads.
	shouldMonitorNetworkStatus = false;
	networkStatusMonitorThread.join();
	closesocket(metadataSocket);																						// Close sockets. This releases used resources.
	closesocket(dataSocket);

	delete[] backBuffer;																								// Release heap-allocated buffers.
	delete[] frontBuffer;

	// TODO: Do you have to release something with the sockaddr's? I thought you did. Look that up.

	backBuffer = nullptr;																								// We use this as the flag for preventing double releases.
}
TelecastStream::~TelecastStream() { if (backBuffer) { close(); } }