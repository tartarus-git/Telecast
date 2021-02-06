// Copyright (c) 2021 tartarus-git
// Licensed under the MIT License.

#include <winsock2.h>					// No need to include Windows.h because Winsock2 already contains the core functionality.
#include <ws2tcpip.h>
#include <thread>

#include "logging/Debug.h"
#include "defines.h"
#include "graphics/gui.h"
#include "storage/Store.h"
#include "remote/discovery.h"

#pragma comment(lib, "Ws2_32.lib")

HWND menu;
int global_nCmdShow; // TODO: Is there a way to avoid the global thing?

bool menuState = false;

// Handle incoming messages.
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		renderGUI();
		return 0;
	case WM_HOTKEY:
		// If correct hotkey is detected, toggle menu and discovery thread.
		if (wParam == HOTKEY_ID) {
			if (menuState) {
				if (!ShowWindow(menu, SW_HIDE)) {			// SW_HIDE is valid because nCmdShow only has to be passed the first call.
					Debug::log("Couldn't hide menu.");
					PostQuitMessage(0);
					return 0;
				}

				// TODO: Tell the thread to stop here before just joining, or else it's never going to work.
				Store::discoverer.join();

				menuState = false;
				return 0;
			}
			if (!ShowWindow(menu, global_nCmdShow)) {			// TODO: Refresh on what nCmdShow is.
				Debug::log("Couldn't show menu.");
				PostQuitMessage(0);
				return 0;
			}

			Store::discoverer = std::thread(discoverDevices);	// TODO: Stop creating a new thread and start reusing.

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
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ wchar_t* lpCmdLine, _In_ int nCmdShow) {
	Debug::log("Initiated program with UNICODE charset.");
#else
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char* lpCmdLine, _In_ int nCmdShow) {
	Debug::log("Initiated program with ANSI charset.");
#endif

	// TODO: Put comment explaining here.
	global_nCmdShow = nCmdShow;

	Debug::log("Creating window class...");
	WNDCLASS windowClass = { };
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = CLASS_NAME;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	Debug::log("Registering window class...");
	RegisterClass(&windowClass);

	Debug::log("Getting screen bounds...");
	// TODO: This will probably totally mess up when faced with a dual monitor system. Find a fix.
	HWND screen = GetDesktopWindow(); // TODO: Research if this is always 0 like in c#.
	RECT bounds;
	if (!GetClientRect(screen, &bounds)) {
		Debug::logError("Failed to get screen bounds.");
		return 0; // TODO: Is this acceptable?
	}

	Debug::log("Calculating window position and size based on screen size.");
	int x = (bounds.right - WINDOW_WIDTH) / 2;
	int y = (bounds.bottom - WINDOW_HEIGHT) / 2;

	Debug::log("Creating window...");
	menu = CreateWindowEx(0, CLASS_NAME, TEXT(""), WS_POPUP, 
		x, y, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);
	// TODO: See about what sort of window resources you have to free after your done with using this stuff.

	// Check whether the menu was created sucessfully.
	if (menu == NULL) {
		Debug::logError("Could not create menu window.");
		return 0;
	}

	Debug::log("Getting device context for menu...");
	Store::g = GetDC(menu);			// TODO: Should I put some sort of safe pointer in here. If memory leaks don't matter after quit, what's the point?

	// Initialize here so that the following code can goto quit.

	sockaddr_in6 local = { };		// TODO: Learn about all the different fields and see if you can optimize this because of setting.
	local.sin6_family = AF_INET6;
	local.sin6_addr = in6addr_any;		// TODO: Is this valid? (for client.)
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then?
	local.sin6_port = htons(CLIENT_PORT);					// Converts from host byte order to network byte order (big-endian).

	int sockOptEnable = 1;
	
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

	// Set up an address for use in broadcasts.
	Store::broadcast = { };
	Store::broadcast.sin6_family = AF_INET6;
	Store::broadcast.sin6_port = htons(CLIENT_PORT);
	if (inet_pton(AF_INET6, "ff02::1", &Store::broadcast.sin6_addr) == SOCKET_ERROR) {					// TODO: See if you can compute this at compile-time because it never changes. constexpr?
		Debug::logError("Failed to convert all nodes multicast socket address from string to INET6_ADDR. Erorr code:\n");
		Debug::logNum(WSAGetLastError());
		goto quit;
	}

	// Initialize the list of discovered devices.
	Store::discoveredDevices = new Device[MAX_DEVICES];					// TODO: Use a safe pointer on this one.
	
	Debug::log("Registering system-wide hotkey for menu...");
	if (!RegisterHotKey(menu, HOTKEY_ID, MOD_SHIFT | MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, KEY_M)) {
		Debug::logError("Failed to register hotkey.");
		goto quit; // TODO: What's the point of freeing the resources at the end of execution. Am I helping the operation system?
		// No you're not, but it's good practice and it forces you to know your memory. It'll be very useful later in life, do it now for niceness's sake. Plus you don't want to fuck anything up when it comes to cross-platform stuff and other OS's.
	}
	
	Debug::log("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

quit:

	Debug::log("Freeing resources...");
	delete[] Store::discoveredDevices;
	ReleaseDC(menu, Store::g);
	// TODO: Check if there is some resource freeing thing that you have to do with Winsock or something.

	Debug::log("Done. Quitting...");
}