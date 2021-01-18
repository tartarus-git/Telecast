// Copyright (c) 2021 tartarus-git
// Licensed under the MIT License.

#pragma once

class Debug
{
public:
	Debug() = delete; // TODO: See if this actually works.

	static void log(const char* message);
	static void logError(const char* message);
};

