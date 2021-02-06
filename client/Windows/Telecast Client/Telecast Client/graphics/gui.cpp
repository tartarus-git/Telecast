#include "gui.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "defines.h"
#include "storage/Store.h"

void renderGUI() {
	while (Store::doGUIRendering) {
		Rectangle(Store::g, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);				// TODO: What's the most efficient way to clear a window with win32?
		// Do some text rendering stuff here.
	}
}