#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

class WebServer {
  private:
    int server_fd;
    int epoll_fd;
    int port;
    std::map<int, std::string> client_buffers;
    Logger lggr;

    static const int MAX_EVENTS = 64;
    static const int BUFFER_SIZE = 4096;

  public:
    WebServer(int p)
        : server_fd(-1), epoll_fd(-1), port(p),
          lggr("webserver.log", Logger::DEBUG, true) {
        lggr.info("An instance of the Webserver was created.");
    }

    ~WebServer() {
        lggr.info("Destroying the instance of the Webserver.");
        cleanup();
    }

    bool initialize() {
        // Create server socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            lggr.error("Failed to create socket\n");
            return false;
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                       sizeof(opt)) == -1) {
            lggr.error("Failed to set socket options\n");
            return false;
        }

        // Make socket non-blocking
        if (!setNonBlocking(server_fd)) {
            return false;
        }

        // Bind socket
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            lggr.error("Failed to bind socket to port " +
                       string_utils::to_string<int>(port) + "\n");
            return false;
        }

        // Listen for connections
        if (listen(server_fd, SOMAXCONN) == -1) {
            lggr.error("Failed to listen on socket\n");
            return false;
        }

        // Create epoll instance
        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            lggr.error("Failed to create epoll instance\n");
            return false;
        }

        // Add server socket to epoll
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = server_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
            lggr.error("Failed to add server socket to epoll\n");
            return false;
        }

        lggr.info("Server initialized on port " +
                  string_utils::to_string(port) + "\n");
        return true;
    }

    void run() {
        struct epoll_event events[MAX_EVENTS];

        lggr.debug("Server running. Waiting for connections...\n");

        while (true) {
            int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
            if (nfds == -1) {
                lggr.error("epoll_wait failed\n");
                break;
            }

            for (int i = 0; i < nfds; i++) {
                if (events[i].data.fd == server_fd) {
                    // New connection
                    handleNewConnection();
                } else {
                    // Data from existing connection
                    handleClientData(events[i].data.fd);
                }
            }
        }
    }

  private:
    bool setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            lggr.error("Failed to get socket flags\n");
            return false;
        }

        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            lggr.error("Failed to set socket non-blocking\n");
            return false;
        }

        return true;
    }

    void handleNewConnection() {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd =
            accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            lggr.error("Failed to accept connection\n");
            return;
        }

        if (!setNonBlocking(client_fd)) {
            close(client_fd);
            return;
        }

        // Add client socket to epoll
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET; // Edge-triggered
        ev.data.fd = client_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            lggr.error("Failed to add client socket to epoll\n");
            close(client_fd);
            return;
        }

        // Initialize client buffer
        client_buffers[client_fd] = "";

        lggr.info("New connection from " +
                  std::string(inet_ntoa(client_addr.sin_addr)) + ":" +
                  string_utils::to_string<unsigned short>(
                      ntohs(client_addr.sin_port)) +
                  " (fd: " + string_utils::to_string<int>(client_fd) + ")\n");
    }

    void handleClientData(int client_fd) {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) >
               0) {
            buffer[bytes_read] = '\0';
            client_buffers[client_fd] += std::string(buffer);

            // Check if we have a complete HTTP request
            if (isCompleteRequest(client_buffers[client_fd])) {
                processRequest(client_fd, client_buffers[client_fd]);
                client_buffers[client_fd].clear();
            }
        }

        if (bytes_read == 0) {
            // Client disconnected
            lggr.info("Client disconnected (fd: " +
                      string_utils::to_string(client_fd) + ")\n");
            closeConnection(client_fd);
        } else if (bytes_read == -1 && errno != EAGAIN &&
                   errno != EWOULDBLOCK) {
            // Error occurred
            lggr.error("Error reading from client (fd: " +
                       string_utils::to_string<int>(client_fd) + ")\n");
            closeConnection(client_fd);
        }
    }

    bool isCompleteRequest(const std::string &request) {
        // Simple check for HTTP request completion
        // Look for double CRLF which indicates end of headers
        return request.find("\r\n\r\n") != std::string::npos;
    }

    void processRequest(int client_fd, const std::string &request) {
        lggr.info("Processing request from fd " +
                  string_utils::to_string(client_fd) + ":\n");
        lggr.debug("--- Request Start ---\n");
        lggr.debug(request);
        lggr.debug("--- Request End ---\n\n");

        // Extract method and path from first line
        size_t first_space = request.find(' ');
        size_t second_space = request.find(' ', first_space + 1);

        if (first_space != std::string::npos &&
            second_space != std::string::npos) {
            std::string method = request.substr(0, first_space);
            std::string path =
                request.substr(first_space + 1, second_space - first_space - 1);

            lggr.info("Method: " + method + ", Path: " + path + "\n");
        }

        // Send a simple HTTP response
        sendResponse(client_fd);
    }

    void sendResponse(int client_fd) {
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: 81\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "<html><body><h1>Don't Panic!"
                               "</h1><p>Your request was "
                               "processed.</p></body></html>";

        ssize_t bytes_sent =
            send(client_fd, response.c_str(), response.length(), 0);
        if (bytes_sent == -1) {
            lggr.error("Failed to send response to client (fd: " +
                       string_utils::to_string(client_fd) + ")\n");
        } else {
            lggr.debug("Sent " + string_utils::to_string(bytes_sent) +
                       " bytes response to fd " +
                       string_utils::to_string(client_fd) + "\n");
        }

        // Close connection after response (HTTP/1.0 style)
        closeConnection(client_fd);
    }

    void closeConnection(int client_fd) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        client_buffers.erase(client_fd);
    }

    void cleanup() {
        if (server_fd != -1) {
            close(server_fd);
        }
        if (epoll_fd != -1) {
            close(epoll_fd);
        }
    }
};

#endif // HTTPSERVER_HPP
