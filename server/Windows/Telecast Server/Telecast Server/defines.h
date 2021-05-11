#pragma once

#define CLASS_NAME "telecast_server"
#define WINDOW_TITLE "Telecast Server"

// TODO: Think about whether having the defines in here makes much sense anymore.

// Main network monitor.
#define MAIN_NETWORK_MONITOR_THREAD_SLEEP 1000										// In milliseconds.

// Discovery.
// TODO: Make sure this doesn't collide with any other protocols and make sure that this is adjustable by user in a file.
#define SERVER_DISCOVERY_PORT 5001													// Receive discovery messages here and respond to them through this one.
#define DISCOVERY_PACKET_KEY 167													// Arbitrary number to mark discovery packets. Makes sure accidental sends to the server port don't always get responded to.
#define DISCOVERER_BUFFER_LENGTH 128												// The amount of discoverers to put into the response buffer before dropping requests.

// Stream.
#define SERVER_DATA_PORT 5002														// Receive UDP stream data through this port.
#define SERVER_DATA_BUFFER_SIZE 1024
#define SERVER_METADATA_PORT 5003													// Receive and send metadata about ongoing connection through TCP.

// Graphics.
#define GRAPHICS_THREAD_SLEEP 1000													// In milliseconds.