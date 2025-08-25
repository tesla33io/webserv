/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:46:05 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/20 15:23:32 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "WebServer.hpp"
#include "Logger/Logger.hpp"
#include "src/ConfigParser/Struct.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"

bool WebServer::_running;
static bool interrupted = false;

WebServer::WebServer(std::vector<ServerConfig> &confs)
    : _epoll_fd(-1),
      _backlog(SOMAXCONN),
      _confs(confs),
      _lggr("ws.log", Logger::DEBUG, true) {
	_lggr.info("An instance of the Webserver was created.");
}

// DEPRECATED?
WebServer::WebServer(std::vector<ServerConfig> &confs, std::string &prefix_path, int log_level)
    : _epoll_fd(-1),
      _backlog(SOMAXCONN),
      _root_prefix_path(prefix_path),
      _confs(confs),
      _lggr("ws.log",
            log_level == 0 ? Logger::ERROR
                           : (log_level == 1     ? Logger::WARNING
                              : (log_level == 2) ? Logger::INFO
                                                 : Logger::DEBUG),
            true) {
	_lggr.info("An instance of the Webserver was created.");
}

WebServer::~WebServer() {
	_lggr.debug("Destroying Webserver instance.");
	cleanup();
}

bool WebServer::initialize() {
	if (!setupSignalHandlers()) {
		return false;
	}

	if (_confs.empty()) {
		_lggr.error("No server configurations provided. Cannot initialize WebServer");
		return false;
	}

	if (!createEpollInstance()) {
		return false;
	}

	for (std::vector<ServerConfig>::iterator it = _confs.begin(); it != _confs.end(); ++it) {
		if (!initializeSingleServer(*it)) {
			return false;
		}
	}

	_running = true;
	return true;
}

void WebServer::run() {
	struct epoll_event events[MAX_EVENTS];
	_last_cleanup = getCurrentTime();

	_lggr.debug("Server running. Waiting for connections...");

	while (_running) {
		int event_count = epoll_wait(_epoll_fd, events, MAX_EVENTS, 100);

		if (event_count == -1 && !interrupted) {
			_lggr.error("epoll_wait failed: " + std::string(strerror(errno)));
			break;
		} else if (event_count == -1 && interrupted) {
			_lggr.warn("Program interrupted, shutting down...");
			break;
		}

		if (event_count > 0) {
			processEpollEvents(events, event_count);
			// _lggr.debug("Processed " + su::to_string(event_count) + " events");
			if (event_count == MAX_EVENTS) {
				_lggr.warn("Hit MAX_EVENTS limit (" + su::to_string(MAX_EVENTS) +
				           "), may have more events pending");
			}
		}

		cleanupExpiredConnections();
	}

	//for (std::vector<ServerConfig>::iterator it = _confs.begin(); it != _confs.end(); ++it) {
	//	if (it->getServerFD() != -1) {
	//		close(it->getServerFD());
	//	}
	//}
}

void sigint_handler(int sig) {
	(void)sig;
	WebServer::_running = false;
	interrupted = true;
}

bool WebServer::setupSignalHandlers() {
	_lggr.debug("Setting up signal handlers");

	if (signal(SIGINT, &sigint_handler) == SIG_ERR) {
		_lggr.error("Failed to set SIGINT handler");
		return false;
	}

	if (signal(SIGTERM, &sigint_handler) == SIG_ERR) {
		_lggr.error("Failed to set SIGTERM handler");
		return false;
	}

	interrupted = false;
	return true;
}

bool WebServer::createEpollInstance() {
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1) {
		_lggr.error("Failed to create epoll instance");
		return false;
	}
	return true;
}

bool WebServer::resolveAddress(const ServerConfig &config, struct addrinfo **result) {
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = getaddrinfo(config.getHost().c_str(), su::to_string<int>(config.getPort()).c_str(),
	                         &hints, result);

	if (status != 0) {
		_lggr.logWithPrefix(Logger::ERROR,
		                    config.getHost() + ":" + su::to_string<int>(config.getPort()),
		                    "Failed to get address info");
		return false;
	}
	return true;
}

bool WebServer::createAndConfigureSocket(ServerConfig &config, const struct addrinfo *addr_info) {
	config.setServerFD(
	    socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol));
	if (config.getServerFD() == -1) {
		_lggr.logWithPrefix(Logger::ERROR,
		                    config.getHost() + ":" + su::to_string<int>(config.getPort()),
		                    "Failed to create socket");
		return false;
	}

	if (!setSocketOptions(config.getServerFD(), config.getHost(), config.getPort())) {
		return false;
	}

	if (!setNonBlocking(config.getServerFD())) {
		return false;
	}

	return true;
}

// https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
bool WebServer::setSocketOptions(int socket_fd, const std::string &host, const int port) {
	int reuse_addr = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1) {
		_lggr.logWithPrefix(Logger::ERROR, host + ":" + su::to_string<int>(port),
		                    "Failed to set SO_REUSEADDR option");
		return false;
	}
	return true;
}

bool WebServer::setNonBlocking(int fd) {
	_lggr.debug("Setting fd [" + su::to_string(fd) + "] as non-blocking");

	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		_lggr.error("Failed to get socket flags for fd " + su::to_string(fd));
		return false;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		_lggr.error("Failed to set socket non-blocking for fd " + su::to_string(fd));
		return false;
	}

	return true;
}

bool WebServer::bindAndListen(const ServerConfig &config, const struct addrinfo *addr_info) {
	if (bind(config.getServerFD(), addr_info->ai_addr, addr_info->ai_addrlen) == -1) {
		_lggr.logWithPrefix(
		    Logger::ERROR, config.getHost() + ":" + su::to_string<int>(config.getPort()),
		    "Failed to bind socket to port " + su::to_string<int>(config.getPort()));
		return false;
	}

	if (listen(config.getServerFD(), _backlog) == -1) {
		_lggr.logWithPrefix(Logger::ERROR,
		                    config.getHost() + ":" + su::to_string<int>(config.getPort()),
		                    "Failed to listen on socket");
		return false;
	}

	return true;
}

bool WebServer::epollManage(int op, int socket_fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = socket_fd;

	if (epoll_ctl(_epoll_fd, op, socket_fd, &ev) == -1) {
		_lggr.error("Tried to " +
		            std::string(op == EPOLL_CTL_ADD   ? "add "
		                        : op == EPOLL_CTL_MOD ? "modify "
		                                              : "delete ") +
		            "fd: " + su::to_string(socket_fd) + " (" + describeEpollEvents(events) +
		            "), but encountered an error (" + std::string(strerror(errno)) + ")");
		return false;
	}
	_lggr.debug("Fd: " + su::to_string(socket_fd) +
	            std::string(op == EPOLL_CTL_ADD   ? " added to epoll instance with mask "
	                        : op == EPOLL_CTL_MOD ? " modified with new mask "
	                                              : " deleted from epoll instance.") +
	            std::string(op == EPOLL_CTL_DEL ? "" : "(" + describeEpollEvents(events) + ")"));

	return true;
}

bool WebServer::initializeSingleServer(ServerConfig &config) {
	struct addrinfo *addr_info = NULL;

	if (!resolveAddress(config, &addr_info)) {
		return false;
	}

	if (!createAndConfigureSocket(config, addr_info)) {
		freeaddrinfo(addr_info);
		return false;
	}

	if (!bindAndListen(config, addr_info)) {
		freeaddrinfo(addr_info);
		return false;
	}

	if (!epollManage(EPOLL_CTL_ADD, config.getServerFD(), EPOLLIN)) {
		freeaddrinfo(addr_info);
		return false;
	}

	freeaddrinfo(addr_info);
	_lggr.logWithPrefix(Logger::INFO, config.getHost() + ":" + su::to_string<int>(config.getPort()),
	                    "Server initialized!");

	return true;
}

void WebServer::cleanup() {
	_lggr.debug("Performing server cleanup...");

	// Close all client connections
	for (std::map<int, Connection *>::iterator it = _connections.begin(); it != _connections.end();
	     ++it) {
		close(it->first);
		delete it->second;
	}
	_connections.clear();

	for (std::vector<ServerConfig>::iterator it = _confs.begin(); it != _confs.end(); ++it) {
		if (it->getServerFD() != -1) {
			close(it->getServerFD());
			it->setServerFD(-1);
		}
	}

	if (_epoll_fd != -1) {
		close(_epoll_fd);
		_epoll_fd = -1;
	}

	_lggr.info("Server cleanup completed");
}

std::string getCurrentWorkingDirectory() {
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		std::cerr << "Warning: could not resolve current working directory." << std::endl;
		return "";
	}
	std::string dir(cwd);
	return dir;
}

int main(int argc, char *argv[]) {
	ArgumentParser ap;
	ServerArgs args;

	try {
		args = ap.parseArgs(argc, argv);
		if (args.show_help) {
			ap.printUsage(argv[0]);
			return 0;
		}
		if (args.show_version) {
			std::cout << __WEBSERV_VERSION__ << std::endl;
			return 0;
		}
		if (args.prefix_path.empty()) {
			args.prefix_path = getCurrentWorkingDirectory();
		}
		std::cout << "prefix_path: " << args.prefix_path << std::endl;
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		std::cerr << "Use --help for usage information." << std::endl;
		return 1;
	}

	ConfigParser configparser(args.log_level);
	std::vector<ServerConfig> servers;

	if (!configparser.loadConfig(args.config_file, servers, args.prefix_path, args.log_level)) {
		std::cerr << "Error: Failed to open or parse configuration file '" << args.config_file
		          << "'" << std::endl;
		std::cerr << "Please check the configuration file syntax and try again." << std::endl;
		return 1;
	}

	WebServer webserv(servers, args.prefix_path, args.log_level);

    webserv.log_level = args.log_level;
	if (!webserv.initialize()) {
		std::cerr << "Failed to initialize web server." << std::endl;
		return 1;
	}

	webserv.run();
	return 0;
}
