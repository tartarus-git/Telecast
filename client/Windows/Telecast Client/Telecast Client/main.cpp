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
	sockaddr_in local = { };		// TODO: Learn about all the different fields and see if you can optimize this because of setting.
	local.sin_family = AF_INET6;
	local.sin_addr.S_un.S_addr = INADDR_ANY;
	local.sin_port = htons(PORT);					// Converts from host byte order to network byte order (big-endian).
	
	Debug::log("Initializing Winsock2...");
	if (WSAStartup(MAKEWORD(2, 2), &Store::wsa) == SOCKET_ERROR) {
		Debug::logError("Failed to initialize Winsock2. Error code:\n");
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
	
	Debug::log("Registering system-wide hotkey for menu...");
	if (!RegisterHotKey(menu, HOTKEY_ID, MOD_SHIFT | MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, KEY_M)) {
		Debug::logError("Failed to register hotkey.");
		goto quit; // TODO: What's the point of freeing the resources at the end of execution. Am I helping the operation system?
	}
	
	Debug::log("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

quit:

	Debug::log("Freeing resources...");
	ReleaseDC(menu, Store::g);

	Debug::log("Done. Quitting...");
}