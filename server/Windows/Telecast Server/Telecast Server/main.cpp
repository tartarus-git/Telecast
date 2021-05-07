#include <winsock2.h>																					// No need to include Windows.h because everything we need is in here.
#include <ws2tcpip.h>

#include <thread>

#include "storage/Store.h"																				// So that we can access important global variables.
#include "networking/networkInit.h"
#include "networking/discovery.h"
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

	LOG("Starting stream metadata thread...");
	std::thread streamMetadataThread();

	LOG("Starting discovery responder thread...");
	std::thread discoveryResponderThread(respondToDiscoveries);													// Start responder thread first so that the buffer can be emptied ASAP when listener is turned on.
	
	LOG("Starting discovery listener thread...");
	std::thread discoveryListenerThread(listenForDiscoveries);													// Start listener thread to listen for discovery broadcasts.

	LOG("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {																		// Run the message loop until we get a WM_QUIT. // TODO: Make sure that's correct.
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	LOG("Message loop ended, program is terminating...");														// End of program.
}