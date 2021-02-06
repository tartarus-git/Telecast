#pragma once

#include <ws2tcpip.h>

struct Device
{
	sockaddr_in6 address;

	bool operator==(const Device& other) const;
};

