#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

bool WebServer::_running;
bool interrupted = false;

WebServer::WebServer(int p)
    : _server_fd(-1), _epoll_fd(-1), _port(p), _backlog(SOMAXCONN),
      _lggr("webserver.log", Logger::DEBUG, true) {
	_lggr.info("An instance of the Webserver was created.");
}

WebServer::~WebServer() {
	_lggr.debug("Destroying the instance of the Webserver.");
	cleanup();
}

void sigint_handler(int sig) {
	(void)sig;
	WebServer::_running = false;
	interrupted = true;
}

// tmp until config parser is done
static std::string getCurrentWorkingDirectory() {
	char buffer[PATH_MAX];
	if (getcwd(buffer, sizeof(buffer)) != NULL) {
		return std::string(buffer);
	} else {
		perror("getcwd failed");
		return std::string();
	}
}

bool WebServer::initialize() {
	_lggr.debug("Overload SIGINT behaviour");
	signal(SIGINT, &sigint_handler);
	_lggr.debug("Overload SIGTERM behaviour");
	signal(SIGTERM, &sigint_handler);
	interrupted = false;

	// Populate address structs
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, string_utils::to_string<int>(_port).c_str(), &hints, &res) != 0) {
		_lggr.error("Failed to get address info");
		return false;
	}

	// Create server socket
	_server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (_server_fd == -1) {
		_lggr.error("Failed to create socket");
		return false;
	}

	// https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ/14388707?newreg=e3fae32d955646afad5169c421fb403a
	int set_true = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &set_true, sizeof(set_true)) == -1) {
		_lggr.error("Failed to set socket options");
		return false;
	}

	// Make socket non-blocking
	if (!setNonBlocking(_server_fd)) {
		return false;
	}

	if (bind(_server_fd, res->ai_addr, res->ai_addrlen) == -1) {
		_lggr.error("Failed to bind socket to port " + string_utils::to_string<int>(_port));
		return false;
	}

	// Listen for connections
	if (listen(_server_fd, _backlog) == -1) {
		_lggr.error("Failed to listen on socket");
		return false;
	}

	// RTFM! `man 7 epoll`
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1) {
		_lggr.error("Failed to create epoll instance");
		return false;
	}

	// Add server socket to epoll
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
	ev.data.fd = _server_fd;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server_fd, &ev) == -1) {
		_lggr.error("Failed to add server socket to epoll");
		return false;
	}

	// Cleanup
	freeaddrinfo(res);

	_lggr.info("Server initialized on port " + string_utils::to_string(_port));
	_running = true;
	_root_path = getCurrentWorkingDirectory();
	if (_root_path.empty()) {
		_lggr.error("Couldn't get current working dir to proceed further");
		return false;
	}
	return _running;
}

static std::string describeEpollEvents(uint32_t ev) {
	std::vector<std::string> bits;
	if (ev & EPOLLIN)
		bits.push_back("EPOLLIN");
	if (ev & EPOLLOUT)
		bits.push_back("EPOLLOUT");
	if (ev & EPOLLPRI)
		bits.push_back("EPOLLPRI");
	if (ev & EPOLLERR)
		bits.push_back("EPOLLERR");
	if (ev & EPOLLHUP)
		bits.push_back("EPOLLHUP");
	if (ev & EPOLLRDHUP)
		bits.push_back("EPOLLRDHUP");
	if (ev & EPOLLONESHOT)
		bits.push_back("EPOLLONESHOT");
	if (ev & EPOLLET)
		bits.push_back("EPOLLET");
	// …add any other flags you care about…
	if (bits.empty())
		return "0";
	std::string s = bits[0];
	for (size_t i = 1; i < bits.size(); ++i)
		s += "|" + bits[i];
	return s;
}

void WebServer::run() {
	struct epoll_event events[MAX_EVENTS];
	_last_cleanup = getCurrentTime();

	_lggr.debug("Server running. Waiting for connections...");

	while (_running) {
		int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, 1000);
		if (nfds == -1 && !interrupted) {
			_lggr.error("epoll_wait failed: " + std::string(strerror(errno)));
			break;
		} else if (nfds == -1 && interrupted) {
			_lggr.warn("Program interrupted, shutting down...");
			break;
		}
		if (nfds == MAX_EVENTS) {
			_lggr.warn("Hit MAX_EVENTS limit (" + string_utils::to_string(MAX_EVENTS) +
			           "), may have more events pending");
		}

		for (int i = 0; i < nfds; i++) {
			uint32_t evmask = events[i].events;
			int fd = events[i].data.fd;
			_lggr.debug("epoll event on fd=" + string_utils::to_string(fd) + " (" +
			            describeEpollEvents(evmask) + ")");
			if (events[i].data.fd == _server_fd) {
				handleNewConnection();
			} else {
				if (_connections.find(events[i].data.fd) != _connections.end()) {
					handleClientData(events[i].data.fd);
				} else {
					_lggr.debug("Ignoring event for unknown fd: " +
					            string_utils::to_string(events[i].data.fd));
				}
			}
		}

		if (nfds > 0) {
			_lggr.debug("Processed " + string_utils::to_string(nfds) + " events");
		}

		cleanupExpiredConnections();

		static time_t last_stats = 0;
		time_t current_time = getCurrentTime();
		if (current_time - last_stats >= 30) { // Every 30 seconds
			logConnectionStats();
			last_stats = current_time;
		}
	}
}

bool WebServer::setNonBlocking(int fd) {
	_lggr.debug("Setting fd [" + string_utils::to_string(fd) + "] as non-blocking");
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		_lggr.error("Failed to get socket flags");
		return false;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		_lggr.error("Failed to set socket non-blocking");
		return false;
	}

	return true;
}

inline time_t WebServer::getCurrentTime() const { return time(NULL); }

bool WebServer::isConnectionExpired(const ConnectionInfo *conn) const {
	time_t current_time = getCurrentTime();
	time_t timeout = conn->keep_alive ? KEEP_ALIVE_TO : CONNECTION_TO;

	return (current_time - conn->last_activity) > timeout;
}

void WebServer::logConnectionStats() {
	size_t active_connections = _connections.size();
	size_t keep_alive_connections = 0;

	for (std::map<int, ConnectionInfo *>::const_iterator it = _connections.begin();
	     it != _connections.end(); ++it) {
		if (it->second->keep_alive) {
			keep_alive_connections++;
		}
	}

	_lggr.info("Connection Stats - Active: " + string_utils::to_string(active_connections) +
	           ", Keep-Alive: " + string_utils::to_string(keep_alive_connections));
}

void WebServer::cleanup() {
	_lggr.debug("Performing server cleanup...");

	for (std::map<int, ConnectionInfo *>::iterator it = _connections.begin();
	     it != _connections.end(); ++it) {
		close(it->first);
		delete it->second;
	}
	_connections.clear();

	if (_server_fd != -1) {
		close(_server_fd);
		_server_fd = -1;
	}

	if (_epoll_fd != -1) {
		close(_epoll_fd);
		_epoll_fd = -1;
	}

	_lggr.info("Server cleanup completed");
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
