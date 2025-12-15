#include "socket_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

/**
 * @file socket_server.c
 * @brief Unix domain socket server implementation
 */

// Receive buffer size
#define RECV_BUF_SIZE 4096

struct SocketServer {
    int listen_fd;              // Listening socket
    int client_fd;              // Connected client (-1 if none)
    char path[108];             // Socket path (max for sun_path)
    char recv_buf[RECV_BUF_SIZE];  // Receive buffer for line assembly
    size_t recv_len;            // Current data in recv_buf
};

/**
 * Set socket to non-blocking mode.
 */
static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

SocketServer* socket_server_create(const char *path) {
    SocketServer *server = calloc(1, sizeof(SocketServer));
    if (!server) return NULL;

    server->client_fd = -1;
    server->recv_len = 0;

    // Use default path if none provided
    if (!path) path = SOCKET_DEFAULT_PATH;
    strncpy(server->path, path, sizeof(server->path) - 1);

    // Remove existing socket file
    unlink(server->path);

    // Create socket
    server->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        perror("socket_server: socket()");
        free(server);
        return NULL;
    }

    // Set non-blocking
    if (!set_nonblocking(server->listen_fd)) {
        perror("socket_server: fcntl()");
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // Bind to path
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, server->path, sizeof(addr.sun_path) - 1);

    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("socket_server: bind()");
        close(server->listen_fd);
        free(server);
        return NULL;
    }

    // Listen for connections
    if (listen(server->listen_fd, 1) < 0) {
        perror("socket_server: listen()");
        close(server->listen_fd);
        unlink(server->path);
        free(server);
        return NULL;
    }

    fprintf(stderr, "Socket server listening on: %s\n", server->path);
    return server;
}

void socket_server_destroy(SocketServer *server) {
    if (!server) return;

    if (server->client_fd >= 0) {
        close(server->client_fd);
    }
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }
    unlink(server->path);
    free(server);
}

/**
 * Try to accept a new connection (non-blocking).
 */
static void try_accept(SocketServer *server) {
    if (server->client_fd >= 0) return;  // Already have a client

    int fd = accept(server->listen_fd, NULL, NULL);
    if (fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("socket_server: accept()");
        }
        return;
    }

    // Set client socket to non-blocking
    if (!set_nonblocking(fd)) {
        perror("socket_server: fcntl() on client");
        close(fd);
        return;
    }

    server->client_fd = fd;
    server->recv_len = 0;
    fprintf(stderr, "Socket client connected\n");
}

/**
 * Close current client connection.
 */
static void close_client(SocketServer *server) {
    if (server->client_fd >= 0) {
        close(server->client_fd);
        server->client_fd = -1;
        server->recv_len = 0;
        fprintf(stderr, "Socket client disconnected\n");
    }
}

bool socket_server_poll(SocketServer *server, char *cmd_buf, size_t buf_size) {
    if (!server || !cmd_buf || buf_size == 0) return false;

    // Try to accept new connection if no client
    try_accept(server);

    if (server->client_fd < 0) return false;

    // Read available data
    size_t space = RECV_BUF_SIZE - server->recv_len - 1;
    if (space > 0) {
        ssize_t n = read(server->client_fd,
                         server->recv_buf + server->recv_len,
                         space);
        if (n > 0) {
            server->recv_len += n;
            server->recv_buf[server->recv_len] = '\0';
        } else if (n == 0) {
            // Client disconnected
            close_client(server);
            return false;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error
            perror("socket_server: read()");
            close_client(server);
            return false;
        }
    }

    // Look for complete line (newline-terminated)
    char *newline = strchr(server->recv_buf, '\n');
    if (!newline) return false;

    // Extract line
    size_t line_len = newline - server->recv_buf;
    if (line_len >= buf_size) {
        // Line too long for buffer - truncate
        line_len = buf_size - 1;
    }
    memcpy(cmd_buf, server->recv_buf, line_len);
    cmd_buf[line_len] = '\0';

    // Remove line from buffer (including newline)
    size_t consumed = (newline - server->recv_buf) + 1;
    if (consumed > server->recv_len) {
        // Shouldn't happen, but protect against buffer corruption
        server->recv_len = 0;
        server->recv_buf[0] = '\0';
        return true;
    }
    size_t remaining = server->recv_len - consumed;
    if (remaining > 0) {
        memmove(server->recv_buf, newline + 1, remaining);
    }
    server->recv_len = remaining;
    server->recv_buf[server->recv_len] = '\0';  // Re-null-terminate

    return true;
}

bool socket_server_send(SocketServer *server, const char *data) {
    if (!server || !data || server->client_fd < 0) return false;

    size_t len = strlen(data);
    bool needs_newline = (len == 0 || data[len - 1] != '\n');

    // Write data
    ssize_t written = write(server->client_fd, data, len);
    if (written < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("socket_server: write()");
            close_client(server);
        }
        return false;
    }

    // Write newline if needed
    if (needs_newline) {
        if (write(server->client_fd, "\n", 1) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close_client(server);
            }
            return false;
        }
    }

    return true;
}

bool socket_server_connected(SocketServer *server) {
    return server && server->client_fd >= 0;
}

const char* socket_server_get_path(SocketServer *server) {
    return server ? server->path : NULL;
}
