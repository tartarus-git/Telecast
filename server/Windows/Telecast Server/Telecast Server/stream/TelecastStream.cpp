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
	backBuffer = new char[SERVER_DATA_BUFFER_SIZE];
	frontBuffer = new char[SERVER_DATA_BUFFER_SIZE];

	// TODO: Is there any way to shrink this long and annoying code into something more portable or something?
	// Stream data socket setup.
	if (dataSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP) == SOCKET_ERROR) {
		LOG("Encountered error while initializing the stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		return;
	}

	if (ioctlsocket(dataSocket, FIONBIO, (u_long*)Store::nonblocking) == SOCKET_ERROR) {			// TODO: It seems like this function tries to modify the nonblocking thing. See if thats true.
		LOG("Encountered error while setting the stream data socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(dataSocket);
		return;
	}

	if (bind(dataSocket, (const sockaddr*)&dataAddress, sizeof(dataAddress)) == SOCKET_ERROR) {
		LOG("Encountered error while binding stream data socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(dataSocket);
		return;
	}

	// Stream metadata socket setup.
	if (metadataSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == SOCKET_ERROR) {
		LOG("Encountered error while initializing the stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		return;												// TODO: Create invalid state if the constructor fails.
	}

	if (ioctlsocket(metadataSocket, FIONBIO, (u_long*)Store::nonblocking) == SOCKET_ERROR) {			// TODO: It seems like this function tries to modify the nonblocking thing. See if thats true.
		LOG("Encountered error while setting the stream metadata socket to non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(metadataSocket);
		return;
	}

	if (bind(metadataSocket, (const sockaddr*)&metadataAddress, sizeof(metadataAddress)) == SOCKET_ERROR) {
		LOG("Encountered error while binding stream metadata socket. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(metadataSocket);
		return;
	}

	dataThread = std::thread(data, this);																				// Start the data and metadata network interfaces.
	metadataThread = std::thread(metadata, this);
}

TelecastStream& TelecastStream::operator=(TelecastStream&& other) noexcept {
	dataThread = std::move(other.dataThread);
	metadataThread = std::move(other.dataThread);
	other.backBuffer = nullptr;
	return *this;
}

void TelecastStream::networkStatusMonitor(TelecastStream* instance) {
	while (instance->shouldMonitorNetworkStatus) {
		if (instance->networkError) {																					// If the networkError flag is set, shutdown data and metadata threads.
			if (instance->shouldReceiveData) {
				instance->shouldReceiveData = false;
				instance->dataThread.join();
				break;
			}
			if (instance->shouldReceiveMetadata) {
				instance->shouldReceiveMetadata = true;
				instance->metadataThread.join();
				break;
			}
		}
	}
	instance->isExperiencingNetworkIssues = true;																		// Set the public network issues flag to true so the main network manager can see it.
}

void TelecastStream::data(TelecastStream* instance) {
	sockaddr_in6 dataClient;								// TODO: Look into IP-spoofing, how easy is it to get past this simple security system.
	int dataClientSize = sizeof(dataClient);

	while (instance->shouldReceiveData) {
		int bytesReceived;
		if ((bytesReceived = recvfrom(instance->dataSocket, instance->backBuffer, SERVER_DATA_BUFFER_SIZE - instance->bufferIndex, 0, (sockaddr*)&dataClient, &dataClientSize)) == SOCKET_ERROR) {					// Try to receive from the UDP data port.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK:
				continue;																															// If there is nothing to receive, just keep trying.
			default:
				LOG("Encountered error while receiving from the stream origin device. Error code: ");
				LOGNUM(error);
				closesocket(instance->dataSocket);
				return;												// TODO: Handle this gracefully and synchronize exit with the metadata socket.
			}
		}

		// If nothing goes wrong, make sure that the data client is the same client as the metadata client.
		// The following has to be checked every time because of UDP socket.
		if (dataClient == instance->currentClient) {															// TODO: Do I have to compare lengths too? Does that make any sense?
			// TODO: The above won't work with !=. Figure out why.
			instance->bufferIndex += bytesReceived;

			if (instance->bufferIndex == SERVER_DATA_BUFFER_SIZE) {												// If the buffer is complete. Do a buffer swap for double buffering and start drawing on the other one.
				while (instance->isFrontBufferValid) { }														// Only swap the buffers if the drawing code if done with the front buffer.
				
				char* temp = instance->frontBuffer;
				instance->frontBuffer = instance->backBuffer;
				instance->backBuffer = temp;

				instance->isFrontBufferValid = true;															// Set flag so that drawing code knows when it's safe to draw the frame.

				instance->bufferIndex = 0;
			}
		}
	}

	// If we shouldn't receive data anymore, dispose all our resources.
	shutdown(instance->dataSocket, SD_BOTH);
	closesocket(instance->dataSocket);
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
		closesocket(instance->metadataSocket);
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
	closesocket(metadataSocket);																						// Close sockets. This releases used resources.
	closesocket(dataSocket);
	backBuffer = nullptr;																								// We use this as the flag for preventing double releases.
}
TelecastStream::~TelecastStream() { if (backBuffer) { close(); } }