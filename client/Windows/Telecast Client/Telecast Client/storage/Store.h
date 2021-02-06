#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <thread>

#include "remote/Device.h"

struct Store
{
	// Drawing surface for the menu window.
	static HDC g;

	// Network.
	static WSADATA wsa;
	static SOCKET s; // TODO: What about freeing this? We have to right, to like free up the port and whatnot?
	static SOCKET discoveryReceiver;							// For receiving answers to a discovery broadcasts.
	static sockaddr_in6 broadcast;

	// List of discovered devices on the network.
	static Device* discoveredDevices;
	static size_t len_discoveredDevices;

	// Threads
	static bool doDiscovery;
	static std::thread discoverer;

	static bool doGUIRendering;
	static std::thread GUIRenderer;
};