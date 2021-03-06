// Copyright (c) 2021 tartarus-git
// Licensed under the MIT License.

// TODO: Obviously add this copyright to the rest of the files as well.

#include <winsock2.h>					// No need to include Windows.h because this already contains everything we need.
#include <ws2tcpip.h>
#include <thread>
#include <CommCtrl.h>

#include "logging/Debug.h"
#include "defines.h"
#include "storage/Store.h"
#include "remote/discovery.h"

#pragma comment(lib, "Ws2_32.lib")

HWND menu;
HWND discoveredDevicesList;

bool menuState = false;

// Handle incoming messages.
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_HOTKEY:
		// If correct hotkey is detected, toggle menu and discovery thread.
		if (wParam == HOTKEY_ID) {
			if (menuState) {

				Store::doDiscovery = false;
				Store::discoverer.join();

				menuState = false;
				return 0;
			}

			Store::doDiscovery = true;
			Store::discoverer = std::thread(discoverDevices);	// TODO: Stop creating a new thread and start reusing. Figure out how to do that. You're going to have to switch library.

			menuState = true;
			return 0;
		}
		// If incorrect hotkey, pass on to default window proc because it is probably a default hotkey for the window.
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Unicode and ANSI compatible entrypoint.
// TODO: Research these _In_ things before the arguments, like why are they useful.
#ifdef UNICODE
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ wchar_t* lpCmdLine, _In_ int nCmdShow) {			// TODO: Think about removing these useless macros in front of params.
	Debug::log("Initiated program with UNICODE charset.");
#else
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char* lpCmdLine, _In_ int nCmdShow) {
	Debug::log("Initiated program with ANSI charset.");
#endif

	Debug::log("Creating window class...");
	WNDCLASS windowClass = { };
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = CLASS_NAME;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	Debug::log("Registering window class...");
	RegisterClass(&windowClass);

	Debug::log("Creating window...");
	menu = CreateWindowEx(0, CLASS_NAME, TEXT("test"), WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
		0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);

	// Check whether the menu was created sucessfully.
	if (menu == NULL) {
		Debug::logError("Could not create menu window.");
		return 0;
	}

	// Set up the local address.
	sockaddr_in6 local = { };		// TODO: Learn about all the different fields and see if you can optimize this because of setting.
	local.sin6_family = AF_INET6;
	local.sin6_addr = in6addr_any;		// TODO: Is this valid? (for client.)
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then?
	local.sin6_port = htons(CLIENT_PORT);					// Converts from host byte order to network byte order (big-endian).

	// Initialize the list of discovered devices.
	Store::discoveredDevices = new Device[MAX_DEVICES];					// TODO: Use a safe pointer on this one.

	u_long nonblocking = true;
	
	Debug::log("Initializing Winsock2...");
	if (WSAStartup(MAKEWORD(2, 2), &Store::wsa) == SOCKET_ERROR) {
		Debug::logError("Failed to initialize Winsock2. Error code:\n");				// TODO: Idk why this \n is in here. Does it look better or something?
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	Debug::log("Creating global UDP socket...");
	if ((Store::s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {
		Debug::logError("Failed to create socket. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	Debug::log("Binding socket to machine...");
	if (bind(Store::s, (const sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		Debug::logError("Failed to bind socket to machine. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	/*Debug::log("Setting socket options so broadcasts can be sent...");														// I think this is only necessary for IPv4 broadcasts, which were not doing right now.
	if (setsockopt(Store::s, SOL_SOCKET, SO_BROADCAST, (const char*)&sockOptEnable, sizeof(int)) == SOCKET_ERROR) {
		Debug::logError("Failed to set socket options. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}*/

	Debug::log("Setting up broadcast address using the all nodes multicast group...");
	Store::broadcast = { };
	Store::broadcast.sin6_family = AF_INET6;
	Store::broadcast.sin6_port = htons(CLIENT_PORT);
	if (inet_pton(AF_INET6, "ff02::1", &Store::broadcast.sin6_addr) == SOCKET_ERROR) {					// TODO: See if you can compute this at compile-time because it never changes. constexpr?
		Debug::logError("Failed to convert all nodes multicast socket address from string to INET6_ADDR. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	Debug::log("Initializing the discovery receiver socket...");
	if ((Store::discoveryListener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		Debug::logError("Failed to create discovery receiver socket. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	Debug::log("Setting the discovery receiver socket to non-blocking...");
	if (ioctlsocket(Store::discoveryListener, FIONBIO, &nonblocking) == SOCKET_ERROR) {										// This is so that we can keep executing if the socket starts waiting for ever or something.
		Debug::logError("Failed to set the discovery receiver socket to non-blocking. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	Debug::log("Binding the discovery receiver socket...");
	if (bind(Store::discoveryListener, (const sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		Debug::logError("Failed to bind the discovery receiver socket. Error code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	Debug::log("Registering system-wide hotkey for menu...");
	if (!RegisterHotKey(menu, HOTKEY_ID, MOD_SHIFT | MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, KEY_M)) {
		Debug::logError("Failed to register hotkey.");
		goto quit;
	}
	
	Debug::log("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

quit:

	Debug::log("Freeing resources...");
	delete[] Store::discoveredDevices;			// TODO: Also definitely free the sockets if you have to here.
	ReleaseDC(menu, Store::g);
	// TODO: Check if there is some resource freeing thing that you have to do with Winsock or something.

	Debug::log("Done. Quitting...");
}