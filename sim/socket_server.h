#ifndef GK_SIM_SOCKET_SERVER_H
#define GK_SIM_SOCKET_SERVER_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file socket_server.h
 * @brief Unix domain socket server for simulator remote control
 *
 * Provides a simple socket server that accepts one client at a time.
 * Commands are received as NDJSON (one JSON object per line).
 * State updates are sent back as NDJSON.
 */

// Default socket path
#define SOCKET_DEFAULT_PATH "/tmp/gatekeeper-sim.sock"

// Opaque server handle
typedef struct SocketServer SocketServer;

/**
 * Create and start socket server.
 * @param path  Unix socket path (NULL for default)
 * @return Server handle, or NULL on error
 */
SocketServer* socket_server_create(const char *path);

/**
 * Destroy socket server and clean up.
 * Closes connections, removes socket file.
 */
void socket_server_destroy(SocketServer *server);

/**
 * Poll for incoming commands (non-blocking).
 * Accepts new connections if none active.
 * Reads available data from connected client.
 *
 * @param server    Server handle
 * @param cmd_buf   Buffer to receive command (one line)
 * @param buf_size  Size of buffer
 * @return true if a complete command was received, false otherwise
 */
bool socket_server_poll(SocketServer *server, char *cmd_buf, size_t buf_size);

/**
 * Send data to connected client.
 * Appends newline if not present.
 *
 * @param server  Server handle
 * @param data    Data to send (typically JSON)
 * @return true on success, false if no client or error
 */
bool socket_server_send(SocketServer *server, const char *data);

/**
 * Check if a client is currently connected.
 */
bool socket_server_connected(SocketServer *server);

/**
 * Get the socket path being used.
 */
const char* socket_server_get_path(SocketServer *server);

#endif /* GK_SIM_SOCKET_SERVER_H */
