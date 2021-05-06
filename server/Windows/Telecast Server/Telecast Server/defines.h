#pragma once

#define CLASS_NAME "telecast_server"
#define WINDOW_TITLE "Telecast Server"

#define SERVER_PORT 5001															// TODO: Make sure this doesn't collide with any other protocols and make sure that this is adjustable by user in a file.

// Discovery.
#define DISCOVERY_PACKET_KEY 167													// Arbitrary number to mark discovery packets. Makes sure accidental sends to the server port don't always get responded to.
#define DISCOVERER_BUFFER_LENGTH 128												// The amount of discoverers to put into the response buffer before dropping requests.