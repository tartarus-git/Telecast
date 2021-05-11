#include "TelecastStream.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>

#include "storage/Store.h"
#include "debugging.h"

#include "defines.h"

#pragma comment(lib, "Ws2_32.lib")

bool operator==(sockaddr_in6 left, sockaddr_in6 right) {							// TODO: This looks insane, there must be a better way, google how to do this one.
																					// I couldn't find anything. I think you need to optimize this yourself once you understand what all these things do and what you don't need.
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

void TelecastStream::invalidate() { backBuffer = nullptr; }																											// Call this with caution, if the back buffer is already allocated, then this will cause memory leak.
bool TelecastStream::isValid() { return backBuffer; }

void TelecastStream::start() {																																		// Actually set up the networking systems required to get a stream up and running.
	networkError = false;																																			// Start with a clean slate so as to not cause any problems for any of the network issue monitoring code.
	shouldMonitorNetworkStatus = true;																																// Start the network monitor so that network issues can be caught.
	networkStatusMonitorThread = std::thread(networkStatusMonitor, this);

	// Stream data socket setup.
	if ((dataSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {																					// Set up a UDP socket for data transmission.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:
			networkError = true;																																	// If network issues turn up, let the network status monitor know.
			return;
		default:
			LOGERROR("Unhandled error while initializing UDP data socket while starting in TelecastStream.", error);
		}
	}

	u_long nonblocking = true;
	if (ioctlsocket(dataSocket, FIONBIO, &nonblocking) == SOCKET_ERROR) {																							// Set the data socket to nonblocking. This is necessary so that our logic can work.
		// NOTE: It looks like the function wants to edit the nonblocking variable, but from what I read in the docs, it doesn't, so don't worry.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:
			closesocket(dataSocket);				// TODO: Handle the possible errors from this thing.
			networkError = true;																																	// If network issues turn up, let the network status monitor know after closing the open socket.
			return;
		default:
			LOGERROR("Unhandled error while setting UDP data socket to nonblocking while starting in TelecastStream.", error);
		}
	}

	if (bind(dataSocket, (const sockaddr*)&dataAddress, sizeof(dataAddress)) == SOCKET_ERROR) {																		// Bind the UDP data socket.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:																																			// If network issues come up, just release all the stuff you need to release and let the network monitor take it from here.
			closesocket(dataSocket);
			networkError = true;
			return;
		default:
			LOGERROR("Unhandled error while binding UDP data socket while starting in TelecastStream.", error);
		}
	}

	// Stream metadata listener socket setup.
	if ((metadataListenerSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR) {																	// Set up a new TCP socket for metadata listening.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:
			closesocket(dataSocket);
			networkError = true;																																	// If network issues turn up, let the network status monitor know after closing the already initialized data socket.
			return;
		default:
			LOGERROR("Unhandled error while initializing TCP metadata listener socket while starting in TelecastStream.", error);
		}
	}

	if (ioctlsocket(metadataListenerSocket, FIONBIO, &nonblocking) == SOCKET_ERROR) {																				// Set the TCP metadata socket to nonblocking.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:
			closesocket(dataSocket);
			closesocket(metadataListenerSocket);
			networkError = true;																																	// If network issues turn up, let the network satus monitor know.
			return;
		default:
			LOGERROR("Unhandled error while setting TCP metadata listener socket to nonblocking while starting in TelecastStream.", error);
		}
	}

	if (bind(metadataListenerSocket, (const sockaddr*)&metadataAddress, sizeof(metadataAddress)) == SOCKET_ERROR) {													// Bind the UDP metadata listener socket.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:																																			// If network issues are already here, let the network monitor handle it after closing both data and metadata sockets.
			closesocket(dataSocket);
			closesocket(metadataListenerSocket);
			networkError = true;
			return;
		default:
			LOGERROR("Unhandled error while binding TCP metadata listener socket while starting in TelecastStream.", error);
		}
	}

	// Start threads.
	dataThread = std::thread(data, this);																															// Start the data thread, handles the raw stream data.
	metadataThread = std::thread(metadata, this);																													// Start the metadata thread, handles the logistics of the transmission.
}

TelecastStream::TelecastStream(u_short dataPort, u_short metadataPort) {
	// Address setup.
	dataAddress = { };																																				// Initialize the address that the data socket uses.
	dataAddress.sin6_family = AF_INET6;
	dataAddress.sin6_addr = in6addr_any;
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then? What is the scope id?
	dataAddress.sin6_port = htons(dataPort);														// Converts from host byte order to network byte order (big-endian).

	metadataAddress = { };																																			// Initialize the address that the metadata listener socket uses.
	metadataAddress.sin6_family = AF_INET6;
	metadataAddress.sin6_addr = in6addr_any;
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then?
	metadataAddress.sin6_port = htons(metadataPort);

	// Allocate enough space on the heap for the display data.
	backBuffer = new char[SERVER_DATA_BUFFER_SIZE];																													// Allocate 2 same-sized buffers so that we can double buffer this.
	frontBuffer = new char[SERVER_DATA_BUFFER_SIZE];

	// Start network monitor.
	networkStatusMonitorThread = std::thread(networkStatusMonitor, this);																							// Start the network monitor. This monitors whatever comes after it for network issues and acts if it finds one.

	// Actual network setup.
	start();																																						// Actually set up the network systems so that we can stream.
}

TelecastStream& TelecastStream::operator=(TelecastStream&& other) noexcept {																			// TODO: This is a lot of stuff to copy, consider making an initialize function so that we don't have to copy everything from the temporary.
	dataAddress = other.dataAddress;
	metadataAddress = other.metadataAddress;

	networkStatusMonitorThread = std::move(other.networkStatusMonitorThread);

	dataThread = std::move(other.dataThread);
	metadataThread = std::move(other.metadataThread);

	frontBuffer = other.frontBuffer;
	backBuffer = other.backBuffer;

	currentClient = other.currentClient;

	size = other.size;

	isFrontBufferValid = other.isFrontBufferValid;

	isExperiencingNetworkIssues = other.isExperiencingNetworkIssues;

	other.backBuffer = nullptr;
	return *this;
}

void TelecastStream::halt() {																							// Shutdown the data and metadata thread.
	shouldReceiveMetadata = false;
	metadataThread.join();																								// There is no release method, we have to wait for the destructors to be called.
	shouldReceiveData = false;
	dataThread.join();
}

void TelecastStream::networkStatusMonitor(TelecastStream* instance) {
	while (instance->shouldMonitorNetworkStatus) {
		// TODO: This is a terrible thread, definitely put a sleep in here. Use chrono probably.

		if (instance->networkError) {																					// If the networkError flag is set, shutdown data and metadata threads.
			instance->halt();
			instance->isExperiencingNetworkIssues = true;																// Set the public network issues flag to true so the main network manager can see it.
			return;
		}
	}
}

void TelecastStream::data(TelecastStream* instance) {
	sockaddr_in6 dataClient;								// TODO: Look into IP-spoofing, how easy is it to get past this simple security system.
	int dataClientSize = sizeof(dataClient);

	int bytesReceived;
	while (instance->shouldReceiveData) {
		if ((bytesReceived = recvfrom(instance->dataSocket, instance->backBuffer, SERVER_DATA_BUFFER_SIZE - instance->bufferIndex, 0, (sockaddr*)&dataClient, &dataClientSize)) == SOCKET_ERROR) {					// Try to receive from the UDP data port.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: case WSAEMSGSIZE: continue;																																						// If there is nothing to receive, just keep trying.
			case WSAENETDOWN: case WSAENETRESET: case WSAETIMEDOUT: goto networkError;																																// If network problems, let this function's network error handler take care of it.
			default:
				LOGERROR("Unhandled error message while receiving data from stream source.", error);
			}
		}

		// If nothing goes wrong, make sure that the data client is the same client as the metadata client.
		// The following has to be checked every time because UDP socket can be sent to by anyone at any time.
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

	// TODO: Add error handling to the following function/functions.
	closesocket(instance->dataSocket);																			// Close the socket.

	return;

networkError:																																				// This should never be reached by normal code execution. Only triggers when a network error occurs somewhere in this function's code.
	closesocket(instance->dataSocket);
	instance->networkError = true;
}

void TelecastStream::metadata(TelecastStream* instance) {
	if (listen(instance->metadataListenerSocket, 1) == SOCKET_ERROR) {										// TODO: Find out if there is a time limit for how long a device can stay in the backlog here.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:																						// If network issues are detected, notify the network monitor thread.
			instance->networkError = true;
			return;
		default:
			LOGERROR("Unhandled error on listen in metadata thread in TelecastStream.", error);
		}
	}
	SOCKET metadataConnection;
	while (instance->shouldReceiveMetadata) {
		// Save the current client for security reasons. We can't be streaming data from someone other than who we're getting metadata from.
		if ((metadataConnection = accept(instance->metadataListenerSocket, (sockaddr*)&instance->currentClient, &instance->currentClientSize)) == INVALID_SOCKET) {			// Be ready to accept a new metadata connection. Save the client so that the UDP data socket knows which client to whitelist.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: case WSAECONNRESET: continue;																												// If nothing is there to accept or if something goes wrong with the accept, just discard the client and try again.
			case WSAENETDOWN: goto networkError;																															// If a network error occurs, goto the network error handling part of this functions code.
			default:
				LOGERROR("Unhandled error while accepting metadata connection in discovery.", error);
			}
		}

		// Once were here, something has been accepted.
		if (recv(metadataConnection, (char*)&instance->size, sizeof(instance->size), 0) == SOCKET_ERROR) {														// Attempt to receive something from the newly created metadata connection.
			int error = WSAGetLastError();
			switch (error) {
			case WSAEWOULDBLOCK: case WSAEMSGSIZE: continue;																									// If the socket doesn't get anything or if the socket gets to much and it doesn't fit in the buffer, just drop whatever it was and try again.
			case WSAENETDOWN: case WSAENETRESET:																												// If there are network issues, goto the network error handling part of this functions code. Close the connection socket first.
				closesocket(metadataConnection);				// TODO: Handle whatever errors could come out of this one.
				goto networkError;
			case WSAETIMEDOUT: case WSAECONNRESET:																												// If something goes wrong with just this single connection, just drop it and accept a new connection.
				closesocket(metadataConnection);
				continue;
			}
		}

		// Once we receive the size, we can open up the stream data port for business.
		instance->shouldReceiveData = true;

		while (instance->shouldReceiveMetadata) {
			continue;
		}

		shutdown(metadataConnection, SD_BOTH);						// TODO: Obviously the SD_BOTH may or may not be necessary depending on what you decide to add to this function.
		closesocket(metadataConnection);
		closesocket(instance->metadataListenerSocket);
		return;
	}
	return;

networkError:																																								// This is only reached if a network error occurs somewhere in the above code. Normal execution should never reach this part.
	closesocket(instance->metadataListenerSocket);																															// Close the metadata listener socket.
	instance->networkError = true;																																			// Notify the network monitor of a network failure.
}

void TelecastStream::close() {
	halt();																																						// Halt data and metadata threads. Sockets get disposed within the threads, so we don't need to do that here.
	shouldMonitorNetworkStatus = false;																															// Start turning off the network monitor thread.
	networkStatusMonitorThread.join();

	delete[] backBuffer;																																		// Release heap-allocated buffers.
	delete[] frontBuffer;

	invalidate();																																				// Invalidate this instance so that the destructor doesn't cause any crazy things to happen if close is called by the user.
}
TelecastStream::~TelecastStream() { if (isValid()) { close(); } }																									// If this class hasn't already been closed, automatically close and dispose everything.