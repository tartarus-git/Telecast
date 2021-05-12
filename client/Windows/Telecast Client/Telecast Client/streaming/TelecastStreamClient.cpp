#include "TelecastStreamClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "debugging.h"

#pragma comment(lib, "Ws2_32.lib")

// Functions to handle and check the state of the object. Useful for checking if the constructor ran successfully or not and for preventing double release.
void TelecastStreamClient::validate() { valid = true; }
void TelecastStreamClient::invalidate() { valid = true; }
bool TelecastStreamClient::isValid() { return valid; }

// Initialize the networking systems.
bool TelecastStreamClient::initialize() {
	// Data socket.
	if ((dataSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return false;																																		// If network error occurs, return false to notify the caller.
		default: LOGERROR("Unhandled error while initializing UDP data socket in TelecastStreamClient initializer.", error);
		}
	}

	// TODO: #define MACRO ((void)0) is unnecessary. #define MACRO just removes it from compilation, use that instead across everything (client and server).

	u_long nonblocking = true;
	if (ioctlsocket(dataSocket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return false;
		default: LOGERROR("Unhandled error while setting UDP data socket to nonblocking in TelecastStreamClient initializer.", error);
		}
	}

	if (bind(dataSocket, (const sockaddr*)&dataAddress, sizeof(dataAddress)) == SOCKET_ERROR) {																				// Bind the discovery listener socket.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return false;
		default: LOGERROR("Unhandled error while binding UDP data socket in TelecastStreamClient initializer.", error);
		}
	}

	// Metadata socket.
	if ((metadataSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return false;																																		// If network error occurs, return false to notify the caller.
		default: LOGERROR("Unhandled error while initializing TDP metadata socket in TelecastStreamClient initializer.", error);
		}
	}

	u_long nonblocking = true;
	if (ioctlsocket(metadataSocket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return false;
		default: LOGERROR("Unhandled error while setting TDP metadata socket to nonblocking in TelecastStreamClient initializer.", error);
		}
	}

	if (bind(metadataSocket, (const sockaddr*)&metadataAddress, sizeof(metadataAddress)) == SOCKET_ERROR) {																				// Bind the discovery listener socket.
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return false;
		default: LOGERROR("Unhandled error while binding TDP metadata socket in TelecastStreamClient initializer.", error);
		}
	}

	return true;																																							// If no errors were encountered along the way, return true.
}

TelecastStreamClient::TelecastStreamClient(u_short dataPort, u_short metadataPort, SIZE frameDimensions) : frameDimensions(frameDimensions) {
	// Frame size calculation.
	frameSize = frameDimensions.cx * 3 * frameDimensions.cy;

	// Address setup.
	dataAddress = { };																										// Data address setup.
	dataAddress.sin6_family = AF_INET6;
	dataAddress.sin6_addr = in6addr_any;
	dataAddress.sin6_port = htons(dataPort);

	metadataAddress = { };																									// Metadata address setup.
	metadataAddress.sin6_family = AF_INET6;
	metadataAddress.sin6_addr = in6addr_any;
	metadataAddress.sin6_port = htons(metadataPort);

	if (initialize()) {
		validate();
		return;
	}
	invalidate();																											// Set the object to an invalid state so as to reflect the fact that the initialization failed.
}

bool TelecastStreamClient::connectToServer(const sockaddr_in6* server) {																											// Connect to a Telecast stream server. This gets called prior to calling sendFrame().
	if (connect(metadataSocket, (const sockaddr*)server, sizeof(sockaddr_in6)) == SOCKET_ERROR) {
		ASSERT(false);
	}
	// If the connection works out, send the frame dimensions over the connection.
	if (send(metadataSocket, (const char*)&frameDimensions, sizeof(frameDimensions), 0) == SOCKET_ERROR) {
		ASSERT(false);
	}
	// If that works out, that means that the data connection is open for business, so we're ready to use it from here on out.
}

bool TelecastStreamClient::sendFrame(const char* data) {
	for (int i = 0; i < frameSize; i += IPV6_MIN_MTU) {							// TODO: We're using the min MTU of IPv6 for now, should work fine, but not optimal.
		// TODO: For each packet, copy to own buffer and add the id in the front.
		// Then just send the thing. It is imperative that no fragmentation occurs.
		// We might have to use SOCK_RAW for the socket to make sure we have full control over this thing.

		// TODO: This is how it works:
		/*
		The minimum MTU (minimum transmission unit (packet size)) for IPv6 is 1280. Use that for now (you need to account for the IPv6 header and extentions and everything).
		In order for a network node to have an MTU which is bigger than the minimum, it needs to be able to discover what the minimum packet size along the target route is.
		If every node on the route has an MTU which is bigger than or equal to the source, no problem. If this isn't the case, the packet won't go through and needs to be retransmitted with different fragmentation.
		To solve this, at the beginning of every TCP session, the network card figures out the minimum MTU along the route and uses that for its fragmentation algorithm.
		The protocol used to discover the min MTU is PMTUD. Since the network card does this automatically, all we need to do is get the value per session from the card and use that to size are packets so that they don't get fragmented.

		For IPv4 this works differently. Packets are fragmented on the go by different network nodes along the route depending on what they support, the PMTUD protocol is still usable in this case though I think.

		PROBLEM: Winsock1 and 2 don't support getting the minimum MTU from the card, but winsock is all that we have because of windows I think. Google if we can use something else.
		SOLUTION: Do PMTUD again but do it yourself for the telecast connection and just recalculate the value that the card already has. It's either this or using the min. MTU defined by the IPv6 standard.
		*/
	}
}