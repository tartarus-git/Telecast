#pragma once

#include <thread>
#include <cstdint>

#include "defines.h"

class TelecastStream {
private:
	uint16_t dataPort;
	uint16_t metadataPort;

	std::thread dataThread;
	std::thread metadataThread;

	static char* buffer;

public:

	static void data();
	static void metadata();

	TelecastStream(uint16_t dataPort, uint16_t metadataPort);

	TelecastStream& operator=(TelecastStream&& other) noexcept;

	void release();
	~TelecastStream();
};