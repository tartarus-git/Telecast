#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

struct Store {
	// WSA data from the WSAStartup() function.
	static WSADATA wsaData;

	// Outward facing main IPv6 of this machine is stored here.
	static sockaddr_in6 local;

	// boolean to reference to set up non-blocking sockets.
	static const u_long nonblocking = true;

	// Discovery broadcasts.
	static SOCKET discoveryListener;																	// UDP
	static SOCKET discoveryResponder;																	// TCP
	static const char* discoveryResponse;

	// Streaming.
	static SOCKET streamDataSocket;																		// UDP
	static SOCKET streamMetadataSocket;																	// TCP
};