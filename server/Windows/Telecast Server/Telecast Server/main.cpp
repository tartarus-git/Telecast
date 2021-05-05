#include <winsock2.h>																					// No need to include Windows.h because everything we need is in here.
#include <ws2tcpip.h>

#include "storage/Store.h"																				// So that we can access important global variables.
#include "debugging.h"																					// Debug helpers.

#include "defines.h"																					// Useful defines.

#pragma comment(lib, "Ws2_32.lib")																		// TODO: Maybe use a macro to make this 32 and 64 bit variable.

// Window messaging callback (window procedure).
LRESULT CALLBACK wndProc(HWND hWnd, unsigned int uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:																					// Handle WM_DESTROY message (post the quit message and return). Basically, just close the window.
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool initNetwork();

// Entrypoint.
#ifdef UNICODE
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nCmdShow) {
	LOG("Entered wWinMain entry point. Using unicode...");
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow) {
	LOG("Entered WinMain entry point. Using ansi...");
#endif

	LOG("Creating window class...");
	WNDCLASS windowClass = { };																					// Create WNDCLASS structure.
	windowClass.lpfnWndProc = wndProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = TEXT(CLASS_NAME);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	LOG("Registering class with OS...");
	RegisterClass(&windowClass);																				// Register the WNDCLASS with the OS. // TODO: Research about ATOMS again.

	LOG("Creating window...");
	HWND mainWindow = CreateWindow(TEXT(CLASS_NAME), TEXT(WINDOW_TITLE), WS_OVERLAPPEDWINDOW | WS_VISIBLE,		// Create the window. This one is visible from the get go so no call to ShowWindow.
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	if (!mainWindow) {																							// If we can't create the window for some reason, just throw error and terminate.
		LOG("Could not create the main window. Terminating...");
		return 0;
	}

	LOG("Initializing networking system...");
	if (!initNetwork()) {
		LOG("Couldn't initialize networking system. Terminating...");
		return 0;																								// Couldn't find anything about needing to release or destroy the window, so I'm not going to.
	}

	LOG("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {																		// Run the message loop until we get a WM_QUIT. // TODO: Make sure that's correct.
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	LOG("Message loop ended, program is terminating...");														// End of program.
}

bool initNetwork() {
	if (WSAStartup(MAKEWORD(2, 2), &Store::wsaData) == SOCKET_ERROR) {											// Initialize WinSock2. // TODO: Find out what this function actually does. Like why is it needed?
		LOG("Failed to initialize WinSock2. Error code: ");
		LOGNUM(WSAGetLastError());
		return false;
	}

	if (Store::discoveryListener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == INVALID_SOCKET) {				// Initialize the discovery listener socket. This will listen for discovery broacasts.
		LOG("Failed to discovery listener socket. Error code: ");
		LOGNUM(WSAGetLastError());
		return false;
	}

	u_long nonblocking = true;
	if (ioctlsocket(Store::discoveryListener, FIONBIO, &nonblocking) == SOCKET_ERROR) {
		LOG("Failed to set the discovery listener socket so non-blocking. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);																	// Free resources of the listener socket before returning.
		return false;
	}

	sockaddr_in6 local = { };																					// Set up the local address for this machine.
	local.sin6_family = AF_INET6;
	local.sin6_addr = in6addr_any;											// TODO: Make sure you understand exactly what this does.
	// TODO: The scope id can be left alone in this case right? Will it just use literal addresses then? What the hell is the scope ID.
	local.sin6_port = htons(SERVER_PORT);																		// Convert server port from host byte order to network byte order (big-endian).

	if (bind(Store::discoveryListener, (const sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {				// Bind discovery listener.
		LOG("Failed to bind the discovery listener to the outward facing IPv6 address and port. Error code: ");
		LOGNUM(WSAGetLastError());
		closesocket(Store::discoveryListener);																	// Free resources of the listener socket before returning.
		return false;
	}

	// No need to join any sort of multicast group. The discovery protocol uses the all nodes multicast group, which all nodes are already a part of.
}