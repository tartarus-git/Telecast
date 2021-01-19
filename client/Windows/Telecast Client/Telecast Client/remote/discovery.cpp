#include "discovery.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "storage/Store.h"

#pragma comment(lib, "Ws2_32.lib")

void discoverDevices() {
	while (Store::doDiscovery) {
		
	}
}