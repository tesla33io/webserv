#include "HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <sys/socket.h>
#include <unistd.h>

WebServer::WebServer(int p)
    : _server_fd(-1), _epoll_fd(-1), _port(p), _backlog(SOMAXCONN),
      _lggr("webserver.log", Logger::DEBUG, true) {
	_lggr.info("An instance of the Webserver was created.");
}

WebServer::~WebServer() {
	_lggr.info("Destroying the instance of the Webserver.");
	cleanup();
}

void sigint_handler(int sig) {
	(void)sig;
	WebServer::_running = false;
}

bool WebServer::initialize() {
	_lggr.debug("Overload SIGINT behaviour\n");
	signal(SIGINT, &sigint_handler);
	_lggr.debug("Overload SIGTERM behaviour\n");
	signal(SIGTERM, &sigint_handler);

	// Populate address structs
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, string_utils::to_string<int>(_port).c_str(), &hints, &res) != 0) {
		_lggr.error("Failed to get address info\n");
		return false;
	}

	// Create server socket
	_server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (_server_fd == -1) {
		_lggr.error("Failed to create socket\n");
		return false;
	}

	// https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ/14388707?newreg=e3fae32d955646afad5169c421fb403a
	int set_true = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &set_true, sizeof(set_true)) == -1) {
		_lggr.error("Failed to set socket options\n");
		return false;
	}

	// Make socket non-blocking
	if (!setNonBlocking(_server_fd)) {
		return false;
	}

	if (bind(_server_fd, res->ai_addr, res->ai_addrlen) == -1) {
		_lggr.error("Failed to bind socket to port " + string_utils::to_string<int>(_port) + "\n");
		return false;
	}

	// Listen for connections
	if (listen(_server_fd, _backlog) == -1) {
		_lggr.error("Failed to listen on socket\n");
		return false;
	}

	// RTFM! `man 7 epoll`
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1) {
		_lggr.error("Failed to create epoll instance\n");
		return false;
	}

	// Add server socket to epoll
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = _server_fd;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server_fd, &ev) == -1) {
		_lggr.error("Failed to add server socket to epoll\n");
		return false;
	}

	_lggr.info("Server initialized on port " + string_utils::to_string(_port) + "\n");
	_running = true;
	return _running;
}

void WebServer::run() {
	struct epoll_event events[MAX_EVENTS];

	_lggr.debug("Server running. Waiting for connections...\n");

	while (_running) {
		int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			_lggr.error("epoll_wait failed\n");
			break;
		}

		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == _server_fd) {
				handleNewConnection();
			} else {
				handleClientData(events[i].data.fd);
			}
		}
	}
}

bool WebServer::setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		_lggr.error("Failed to get socket flags\n");
		return false;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		_lggr.error("Failed to set socket non-blocking\n");
		return false;
	}

	return true;
}

void WebServer::handleNewConnection() {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd == -1) {
		_lggr.error("Failed to accept connection\n");
		return;
	}

	if (!setNonBlocking(client_fd)) {
		close(client_fd);
		return;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = client_fd;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
		_lggr.error("Failed to add client socket (fd: " + string_utils::to_string<int>(client_fd) +
		            ") to epoll\n");
		close(client_fd);
		return;
	}

	// Initialize client buffer
	_client_buffers[client_fd] = "";

	_lggr.info("New connection from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" +
	           string_utils::to_string<unsigned short>(ntohs(client_addr.sin_port)) +
	           " (fd: " + string_utils::to_string<int>(client_fd) + ")\n");
}

void WebServer::handleClientData(int client_fd) {
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;

	while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
		buffer[bytes_read] = '\0';
		_client_buffers[client_fd] += std::string(buffer);

		if (isCompleteRequest(_client_buffers[client_fd])) {
			processRequest(client_fd, _client_buffers[client_fd]);
			_client_buffers[client_fd].clear();
		}
	}

	if (bytes_read == 0) {
		_lggr.info("Client disconnected (fd: " + string_utils::to_string(client_fd) + ")\n");
		closeConnection(client_fd);
	} else if (bytes_read == -1) {
		//} else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
		_lggr.error("Error reading from client (fd: " + string_utils::to_string<int>(client_fd) +
		            ")\n");
		closeConnection(client_fd);
	}
}

bool WebServer::isCompleteRequest(const std::string &request) {
	// Simple check for HTTP request completion
	// Look for double CRLF which indicates end of headers
	return request.find("\r\n\r\n") != std::string::npos;
}

void WebServer::processRequest(int client_fd, const std::string &request) {
	_lggr.info("Processing request from fd " + string_utils::to_string(client_fd) + ":\n");
	_lggr.debug("--- Request Start ---\n");
	_lggr.debug(request);
	_lggr.debug("--- Request End ---\n\n");

	// Extract method and path from first line
	size_t first_space = request.find(' ');
	size_t second_space = request.find(' ', first_space + 1);
	struct Request req;

	if (first_space != std::string::npos && second_space != std::string::npos) {
		req.method = request.substr(0, first_space);
		req.path = request.substr(first_space + 1, second_space - first_space - 1);
		req.clfd = client_fd;

		_lggr.info("Method: " + req.method + ", Path: " + req.path + "\n");
	}

	// Send a simple HTTP response
	sendResponse(req);
}

void WebServer::sendResponse(const Request &req) {
	bool close_conn = true;
	std::string response;
	std::ifstream file;

	if (req.method == "GET") {
		_lggr.debug("Requested path: " + req.path);
		file.open(req.path.c_str(), std::ios::in | std::ios::binary);

		if (!file.is_open()) {
			_lggr.error("[Resp] File not found: " + req.path);
			response = "HTTP/1.1 404 Not Found\r\n"
			           "Content-Type: text/plain\r\n"
			           "Content-Length: 13\r\n\r\n"
			           "404 Not Found";
		} else {
			std::stringstream buffer;
			buffer << file.rdbuf();
			file.close();

			// TODO: content-type detection
			std::string fileContent = buffer.str();
			response = "HTTP/1.1 200 OK\r\n"
			           "Content-Type: text/plain,text/html\r\n"
			           "Content-Length: " +
			           string_utils::to_string<int>(fileContent.size()) + "\r\n\r\n" + fileContent;
		}
	} else if (req.method == "POST" || req.method == "DELETE") {
		response = "HTTP/1.1 501 Not Implemented\r\n"
		           "Content-Type: application/json\r\n"
		           "Content-Length: 42\r\n\r\n"
		           "{\"status\": 501,\"error\": \"Not Implemented\"}";
	} else {
		response = "HTTP/1.1 405 Method Not Allowed\r\n"
		           "Content-Type: text/plain\r\n"
		           "Content-Length: 23\r\n\r\n"
		           "405 Method Not Allowed";
	}

	ssize_t bytes_sent = send(req.clfd, response.c_str(), response.size(), 0);
	if (bytes_sent < 0) {
		_lggr.error("Failed to send response to client (fd: " + string_utils::to_string(req.clfd) +
		            ")");
	} else {
		_lggr.debug("Sent " + string_utils::to_string(bytes_sent) + " bytes response to fd " +
		            string_utils::to_string(req.clfd));
	}

	// TODO: handle headers such as `keep-alive` connection

	if (close_conn) {
		closeConnection(req.clfd);
	}
}

void WebServer::closeConnection(int client_fd) {
	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
	close(client_fd);
	_client_buffers.erase(client_fd);
}

void WebServer::cleanup() {
	if (_server_fd != -1) {
		close(_server_fd);
	}
	if (_epoll_fd != -1) {
		close(_epoll_fd);
	}
}

int main(int argc, char *argv[]) {
	int port = 8080;

	if (argc > 1) {
		port = atoi(argv[1]);
		if (port <= 0 || port > 65535) {
			std::cerr << "Invalid port number. Using default port 8080.\n";
			port = 8080;
		}
	}

	WebServer server(port);

	if (!server.initialize()) {
		std::cerr << "Failed to initialize server\n";
		return 1;
	}

	server.run();

	return 0;
}
