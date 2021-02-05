#pragma once

#define WIN32_LEAN_AND_MEAN				// Define this to prevent clashes with Winsock2.
#include <Windows.h>

// Menu registration.
#define CLASS_NAME TEXT("Telecast_Menu")

// Menu.
#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 600

// Networking.
#define CLIENT_PORT 5000				// TODO: Make sure that there are no collisions between these ports and some other well-known protocol. Also make a config file where they are changeable.
#define SERVER_PORT 5001

#define FRAG_VALUE 2000
#define FRAG_DATA FRAG_VALUE - 4 // TODO: Research about automatic brackets on defines.

// Hotkey.
#define HOTKEY_ID 0
#define KEY_M 0x4D