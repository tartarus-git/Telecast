// Very simple debugging header, which only causes overhead in debug mode. In release mode, all the debugging stuff dissapears from the code.

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>																						// Include lean and mean Windows.h for access to debugapi.h.

#include <cstdint>

inline void log(const char* message) {
	OutputDebugStringA(message);
}

#define NUM_BUFFER_SIZE 100
#define ASCII_NUMBERS_BEGIN 0x30

static char numBuffer[NUM_BUFFER_SIZE];								// TODO: Fit this array to the minimum size for an int32_t.
inline void logNum(int32_t num) {
	// Not implemented yet.																					// TODO: BTW, research how exactly exceptions work in c++.
	
	bool isNegative;
	if (num < 0) {
		num = -num;
		isNegative = true;
	}
	else { isNegative = false; }

	int index = NUM_BUFFER_SIZE - 3;
	numBuffer[NUM_BUFFER_SIZE - 2] = '\n';																						// Insert a newline and a NUL character at what will be the end of the string.
	numBuffer[NUM_BUFFER_SIZE - 1] = '\0';
	while (true) {														// TODO: Is this the best way to do this? Look it up online and see what other possibilities there are.
		int32_t temp = num;
		num /= 10;
		if (!num) {
			numBuffer[index] = ASCII_NUMBERS_BEGIN + temp;
			break;
		}
		numBuffer[index] = ASCII_NUMBERS_BEGIN + (temp - num * 10);
		index--;
	}
	if (isNegative) { numBuffer[index - 1] = '-'; }

	log(&numBuffer[index]);																								// Print from the start of the filled section of the num buffer to the end.
}

inline void logError(const char* message, int errorCode) {
	log("ERROR: ");
	log(message);
	log(" | Error code: ");
	logNum(errorCode);
}

#ifdef _DEBUG
#define LOG(message) log(message "\n")
#define LOGNUM(num) logNum(num)

#define LOGERROR(message, errorCode) logError(message, errorCode); __debugbreak();

#define ASSERT(condition) if (!(condition)) { __debugbreak(); }
#else
#define LOG(message) ((void)0)
#define LOGNUM(num) ((void)0)

#define LOGERROR(message, errorCode) ((void)0)

#define ASSERT(condition) ((void)0)
#endif