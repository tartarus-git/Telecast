#include <winsock2.h>																					// No need to include Windows.h because everything we need is in here.
#include <ws2tcpip.h>

#include <thread>
#include <chrono>

#include "storage/Store.h"																				// So that we can access important global variables.
#include "networking/discovery.h"
#include "stream/TelecastStream.h"
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

TelecastStream mainStream;
std::thread discoveryResponderThread;
std::thread discoveryListenerThread;

bool shouldMonitorForNetworkErrors = true;
void monitorForNetworkErrors() {																		// Thread, which resets necessary systems when network errors transpire.
	while (shouldMonitorForNetworkErrors) {
		if (!shouldDiscoveryListenRun) {																// If the discovery listener exits prematurely, synchronize exit across all systems.
			discoveryListenerThread.join();
			shouldDiscoveryRespondRun = false;
			discoveryResponderThread.join();
			mainStream.halt();
		}
		else if (!shouldDiscoveryRespondRun) {															// If the discovery responder exits prematurely, synchronize exit across all systems.
			discoveryResponderThread.join();
			shouldDiscoveryListenRun = false;
			discoveryListenerThread.join();
			mainStream.halt();
		}
		else if (mainStream.isExperiencingNetworkIssues) {												// If the stream is experiencing network issues, synchronize exit across all systems.
			shouldDiscoveryListenRun = false;
			discoveryListenerThread.join();
			shouldDiscoveryRespondRun = false;
			discoveryResponderThread.join();
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(MAIN_NETWORK_MONITOR_THREAD_SLEEP));
			continue;
		}
		ASSERT(false);																					// Wait for the network to return to a usable state.
		mainStream.start();																				// Restart the stream.
		discoveryResponderThread = std::thread(listenForDiscoveries);									// Restart the discovery responder and listener.
		discoveryListenerThread = std::thread(respondToDiscoveries);
	}
}

HWND window;

bool shouldGraphicsLoopRun = true;
void graphicsLoop();

// Entrypoint.
#ifdef UNICODE
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nCmdShow) {
	LOG("Entered wWinMain entry point. Using unicode...");
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow) {
	LOG("Entered WinMain entry point. Using ANSI...");
#endif

	LOG("Initializing networking system...");
	if (WSAStartup(MAKEWORD(2, 2), &Store::wsaData) == SOCKET_ERROR) {											// Initialize WinSock2. // TODO: Find out what this function actually does. Like why is it needed?
		LOG("Failed to initialize WinSock2. Error code: ");
		LOGNUM(WSAGetLastError());
		return 0;
	}

	LOG("Creating window class...");
	WNDCLASS windowClass = { };																					// Create WNDCLASS structure.
	windowClass.lpfnWndProc = wndProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = TEXT(CLASS_NAME);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	LOG("Registering class with OS...");
	RegisterClass(&windowClass);																				// Register the WNDCLASS with the OS. // TODO: Research about ATOMS again.

	LOG("Creating window...");
	window = CreateWindow(TEXT(CLASS_NAME), TEXT(WINDOW_TITLE), WS_OVERLAPPEDWINDOW | WS_VISIBLE,				// Create the window. This one is visible from the get go so no call to ShowWindow.
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	if (!window) {																								// If we can't create the window for some reason, just throw error and terminate.
		LOG("Could not create the main window. Terminating...");
		return 0;
	}

	LOG("Initializing graphics...");
	std::thread graphicsThread(graphicsLoop);

	LOG("Starting network error monitoring system...");
	std::thread networkMonitoringThread(monitorForNetworkErrors);

	LOG("Starting the Telecast stream...");
	mainStream = TelecastStream(SERVER_DATA_PORT, SERVER_METADATA_PORT);

	LOG("Initializing discovery address...");
	initializeLocalDiscoveryAddress();

	LOG("Starting discovery responder thread...");
	discoveryResponderThread = std::thread(respondToDiscoveries);												// Start responder thread first so that the buffer can be emptied ASAP when listener is turned on.
	
	LOG("Starting discovery listener thread...");
	discoveryListenerThread = std::thread(listenForDiscoveries);												// Start listener thread to listen for discovery broadcasts.

	LOG("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {																		// Run the message loop until we get a WM_QUIT. // TODO: Make sure that's correct.
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	LOG("Message loop ended, program is terminating...");														// Close and release everything before the program terminates.
	shouldDiscoveryListenRun = false;																			// Shutdown discovery listening and responding.
	discoveryListenerThread.join();
	shouldDiscoveryRespondRun = false;
	discoveryResponderThread.join();

	shouldGraphicsLoopRun = false;																				// Shutdown the graphics system.
	graphicsThread.join();
	
	mainStream.close();																							// Shutdown the main data stream.

	shouldMonitorForNetworkErrors = false;																		// Shutdown the network monitoring system.
	networkMonitoringThread.join();

	if (WSACleanup() == SOCKET_ERROR) {
		int error = WSAGetLastError();
		switch (error) {
		case WSAENETDOWN: return 0;
		default:
			LOGERROR("Unhandled error after calling WSACleanup().", error);
		}
	}
}

void graphicsLoop() {
	HDC finalG = GetDC(window);																						// Get the context of the window.
	HDC g = CreateCompatibleDC(finalG);																				// Create memory context for the sake of copying the front buffer to a context through the bitmap.
	HBITMAP bmp = CreateCompatibleBitmap(finalG, mainStream.size.cx, mainStream.size.cy);							// This doesn't work directly with finalG because it already has a bitmap selected into it.
	SelectObject(g, bmp);																							// TODO: Make sure you understand the above correctly.

	while (shouldGraphicsLoopRun) {
		if (mainStream.isFrontBufferValid) {
			if (!SetBitmapBits(bmp, SERVER_DATA_BUFFER_SIZE, mainStream.data)) {					// TODO: Make sure to remember that the data must be WORD aligned.
				LOG("Failed to set the bitmap bits to the front buffer. Terminating...");			// TODO: Figure out how to make this break out of this loop terminate all the threads (the whole program).
				break;
			}
			mainStream.isFrontBufferValid = false;
			// Copy one the memoryDC to the actual window DC.
			// Use zoom algorithm to fit the received image onto whatever actual dimensions we've got for the window.
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(GRAPHICS_THREAD_SLEEP));
	}

	DeleteObject(bmp);
	DeleteDC(g);
	ReleaseDC(window, g);																						// Release the context of the window.
}