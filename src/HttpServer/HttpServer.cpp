#include "ConfigParser/config_parser.hpp"
#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

bool WebServer::_running;
static bool interrupted = false;

WebServer::WebServer(int port)
    : _host("127.0.0.1"), _server_fd(-1), _epoll_fd(-1), _port(port), _backlog(SOMAXCONN),
      _lggr("ws_" + _host + "_" + su::to_string(port) + ".log", Logger::DEBUG, true) {
	_lggr.info("An instance of the Webserver was created.");
}

WebServer::WebServer(std::string &host, int port)
    : _host(host), _server_fd(-1), _epoll_fd(-1), _port(port), _backlog(SOMAXCONN),
      _lggr("ws_" + _host + "_" + su::to_string(port) + ".log", Logger::DEBUG, true) {
	_lggr.info("An instance of the Webserver was created.");
}

WebServer::WebServer(std::vector<ServerConfig> &confs)
    : _host("127.0.0.1"), _server_fd(-1), _epoll_fd(-1), _port(0), _backlog(SOMAXCONN),
      _confs(confs), _lggr("ws.log", Logger::DEBUG, true) {
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

	if (_confs.size() < 1) {
		_lggr.error("No server configs were passed. Cannot initialize Webserv");
		return false;
	}

	// RTFM! `man 7 epoll`
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1) {
		_lggr.error("Failed to create epoll instance");
		return false;
	}

	for (std::vector<ServerConfig>::iterator scit = _confs.begin(); scit != _confs.end(); ++scit) {
		// Populate address structs
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;

		if (getaddrinfo(scit->host.c_str(), su::to_string<int>(scit->port).c_str(), &hints, &res) !=
		    0) {
			_lggr.logWithPrefix(Logger::ERROR, scit->host, "Failed to get address info");
			return false;
		}

		// Create server socket
		scit->server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (scit->server_fd == -1) {
			_lggr.logWithPrefix(Logger::ERROR, scit->host, "Failed to create socket");
			return false;
		}

		// https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ/14388707?newreg=e3fae32d955646afad5169c421fb403a
		int set_true = 1;
		if (setsockopt(scit->server_fd, SOL_SOCKET, SO_REUSEADDR, &set_true, sizeof(set_true)) ==
		    -1) {
			_lggr.logWithPrefix(Logger::ERROR, scit->host, "Failed to set socket options");
			return false;
		}

		// Make socket non-blocking
		if (!setNonBlocking(scit->server_fd)) {
			return false;
		}

		if (bind(scit->server_fd, res->ai_addr, res->ai_addrlen) == -1) {
			_lggr.logWithPrefix(Logger::ERROR, scit->host,
			                    "Failed to bind socket to port " + su::to_string<int>(scit->port));
			return false;
		}

		// Listen for connections
		if (listen(scit->server_fd, _backlog) == -1) {
			_lggr.logWithPrefix(Logger::ERROR, scit->host, "Failed to listen on socket");
			return false;
		}

		// Add server socket to epoll
		struct epoll_event ev;
		ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
		ev.data.fd = scit->server_fd;

		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, scit->server_fd, &ev) == -1) {
			_lggr.logWithPrefix(Logger::ERROR, scit->host, "Failed to add server socket to epoll");
			return false;
		}

		// Cleanup
		freeaddrinfo(res);
		_lggr.logWithPrefix(Logger::INFO, scit->host,
		                    "Server initialized on port " + su::to_string(scit->port));
	}

	_running = true;
	_max_content_length = 8192; // TODO: replace with the value from config
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
	if (bits.empty())
		return "0";
	std::string s = bits[0];
	for (size_t i = 1; i < bits.size(); ++i)
		s += "|" + bits[i];
	return s;
}

static bool isServerFD(std::vector<ServerConfig> &confs, int fd,
                       std::vector<ServerConfig> *pendingVect = NULL) {
	for (std::vector<ServerConfig>::iterator it = confs.begin(); it != confs.end(); ++it)
		if (fd == it->server_fd) {
			if (pendingVect)
				pendingVect->push_back(*it);
			return true;
		}
	return false;
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
			_lggr.warn("Hit MAX_EVENTS limit (" + su::to_string(MAX_EVENTS) +
			           "), may have more events pending");
		}

		for (int i = 0; i < nfds; i++) {
			uint32_t evmask = events[i].events;
			int fd = events[i].data.fd;
			_lggr.debug("epoll event on fd=" + su::to_string(fd) + " (" +
			            describeEpollEvents(evmask) + ")");
			if (isServerFD(_confs, events[i].data.fd, &_have_pending_conn)) {
				handleNewConnection();
			} else {
				if (_connections.find(events[i].data.fd) != _connections.end()) {
					handleClientData(events[i].data.fd);
				} else {
					_lggr.debug("Ignoring event for unknown fd: " +
					            su::to_string(events[i].data.fd));
				}
			}
		}

		if (nfds > 0) {
			_lggr.debug("Processed " + su::to_string(nfds) + " events");
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
	_lggr.debug("Setting fd [" + su::to_string(fd) + "] as non-blocking");
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

	_lggr.info("Connection Stats - Active: " + su::to_string(active_connections) +
	           ", Keep-Alive: " + su::to_string(keep_alive_connections));
}

void WebServer::cleanup() {
	// TODO: adjust cleanup routine to work with new ServerConfig system
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
	Logger lgger("config_prasing", Logger::INFO, true);

	if (argc != 2) {
		std::cerr << "TODO: write a nice error message with usage instructions here <<<"
		          << std::endl;
		return 1;
	}

	ConfigNode conf;
	if (!ConfigParsing::tree_parser(argv[1], conf, lgger)) {
		std::cerr << "TODO: also nice error message here..." << std::endl;
		return 1;
	}

	std::vector<ServerConfig> servers;
	ConfigParsing::struct_parser(conf, servers, lgger);
	WebServer Wserver(servers);

	Wserver.initialize();
	Wserver.run();

	// int port = 8080;

	// if (argc > 1) {
	//	port = atoi(argv[1]);
	//	if (port <= 0 || port > 65535) {
	//		std::cerr << "Invalid port number. Using default port 8080.\n";
	//		port = 8080;
	//	}
	// }

	// WebServer server(port);

	// if (!server.initialize()) {
	//	std::cerr << "Failed to initialize server\n";
	//	return 1;
	// }

	// server.run();

	return 0;
}
