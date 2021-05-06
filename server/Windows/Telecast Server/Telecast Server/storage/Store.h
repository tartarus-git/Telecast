#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

struct Store {
	// WSA data from the WSAStartup() function.
	static WSADATA wsaData;

	// Discovery broadcasts.
	static SOCKET discoveryListener;																	// UDP
	static SOCKET discoveryResponder;																	// TCP
	static const char* discoveryResponse;
};