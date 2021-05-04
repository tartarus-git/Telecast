// Copyright (c) 2021 tartarus-git
// Licensed under the MIT License.

#include "Debug.h"

// Include Windows.h for access to debugapi.h.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdio.h>

char Debug::numBuffer[NUM_BUFFER_LENGTH];

// Logs the message to the debug output. Doesn't do anything when not in debug mode.
void Debug::log(const char* message) {
#ifdef _DEBUG
	size_t length = strlen(message);

	// TODO: Research if memory leaks become irrelevant if the program quites.
	char* buffer = new char[length + 2];
	memcpy(buffer, message, length);
	buffer[length] = '\n';
	buffer[length + 1] = '\0';

	OutputDebugStringA(buffer);
#endif
}

void Debug::logError(const char* message) {
#ifdef _DEBUG
	size_t length = strlen(message);
	char* buffer = new char[1 + 7 + length + 2];
	buffer[0] = '\t';
	memcpy(buffer + 1, "ERROR:", 7);
	memcpy(buffer + 8, message, length);
	buffer[length] = '\n';
	buffer[length] = '\0';

	OutputDebugStringA(buffer);
#endif
}

void Debug::logNum(int num) {
	if (sprintf_s(numBuffer, NUM_BUFFER_LENGTH, "%d", num) < 0) {
		logError("Failure while logging number.");
		return;
	}
	log(numBuffer);
}