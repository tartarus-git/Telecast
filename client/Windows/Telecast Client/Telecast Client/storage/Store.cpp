#include "Store.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <thread>

// TODO: Make sure this stuff is actually necessary.

HDC Store::g;

WSADATA Store::wsa;
SOCKET Store::s;
SOCKET Store::discoveryListener;
sockaddr_in6 Store::broadcast;

Device* Store::discoveredDevices;
size_t Store::len_discoveredDevices = 0;

bool Store::doDiscovery;
std::thread Store::discoverer;

bool Store::doGUIRendering;
std::thread Store::GUIRenderer;