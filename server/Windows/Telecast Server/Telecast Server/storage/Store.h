#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

struct Store {
	// WSA data from the WSAStartup() function.
	static WSADATA wsaData;

	// Boolean to reference to in order to set up non-blocking sockets.
	static const u_long nonblocking = true;
};