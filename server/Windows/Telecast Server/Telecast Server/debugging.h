// Very simple debugging header, which only causes overhead in debug mode. In release mode, all the debugging stuff dissapears from the code.

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>																						// Include lean and mean Windows.h for access to debugapi.h.

inline void log(const TCHAR* message) {
	OutputDebugString(message);
}
inline void logNum(int num) {
	// Not implemented yet.																					// TODO: BTW, research how exactly exceptions work in c++.
}

#ifdef _DEBUG
#define LOG(message) log(TEXT(message "\n"))
#define LOGNUM(num) logNum(num)

#define ASSERT(condition) if (!(condition)) { __debugbreak(); }
#else
#define LOG(message) ((void)0)
#define LOGNUM(num) ((void)0)

#define ASSERT(condition) ((void)0)
#endif