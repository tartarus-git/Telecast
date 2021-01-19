// Copyright (c) 2021 tartarus-git
// Licensed under the MIT License.

#pragma once

#define NUM_BUFFER_LENGTH 100

class Debug
{
	static char numBuffer[NUM_BUFFER_LENGTH];

public:
	Debug() = delete; // TODO: See if this actually works.

	static void log(const char* message);
	static void logError(const char* message);
	static void logNum(int num);
};

