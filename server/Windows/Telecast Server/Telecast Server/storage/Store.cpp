#include "Store.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

WSADATA Store::wsaData;

SOCKET Store::discoveryListener;
SOCKET Store::discoveryResponder;
const char* Store::discoveryResponse = "test";