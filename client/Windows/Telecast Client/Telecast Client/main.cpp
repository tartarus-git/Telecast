// Copyright (c) 2021 tartarus-git
// Licensed under the MIT License.

#include <Windows.h>

#include "logging/Debug.h"
#include "defines.h"
#include "gui.h"

// Handle incoming messages.
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		renderGUI(); // TODO: Flesh this out.
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Unicode and ANSI compatible entrypoint.
// TODO: Research these _In_ things before the arguments, like why are they useful.
#ifdef UNICODE
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ wchar_t* lpCmdLine, _In_ int nCmdShow) {
	Debug::log("Initiated program with UNICODE charset.");
#else
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char* lpCmdLine, _In_ int nShowCmd) {
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
	HWND menu = CreateWindowEx(0, CLASS_NAME, TEXT(""), WS_POPUP, 
		x, y, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);
	// TODO: See about what sort of window resources you have to free after your done with using this stuff.

	// Check whether the menu was created sucessfully.
	if (menu == NULL) {
		Debug::logError("Could not create menu window.");
		return 0;
	}

	Debug::log("Running message loop...");
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}