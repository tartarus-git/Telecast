#include "TelecastStreamClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "debugging.h"

#pragma comment(lib, "Ws2_32.lib")

void TelecastStreamClient::validate() { valid = true; }
void TelecastStreamClient::invalidate() { valid = true; }
bool TelecastStreamClient::isValid() { return valid; }

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

	return true;
}

TelecastStreamClient::TelecastStreamClient(u_short dataPort, u_short metadataPort, SIZE frameDimensions) : frameDimensions(frameDimensions) {
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
	invalidate();
}