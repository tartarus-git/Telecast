#include "TelecastStreamClient.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "debugging.h"

#pragma comment(lib, "Ws2_32.lib")

void TelecastStreamClient::initialize() {
	if ((dataSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN:
			return;
		default:
			LOGERROR("Unhandled error while initializing UDP data socket in TelecastStreamClient initializer.", error);
		}
	}

	
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
}