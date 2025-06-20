#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <signal.h>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>

class WebServer {

  public:
	WebServer(int p);
	~WebServer();

	bool initialize();
	void run();
	
	static bool _running;

  private:
	int _server_fd;
	int _epoll_fd;
	int _port;
	int _backlog;
	std::map<int, std::string> _client_buffers;
	Logger _lggr;

	static const int MAX_EVENTS = 64;
	static const int BUFFER_SIZE = 4096;

	bool setNonBlocking(int fd);
	void handleNewConnection();
	void handleClientData(int client_fd);
	bool isCompleteRequest(const std::string &request);
	void processRequest(int client_fd, const std::string &request);
	void sendResponse(int client_fd);
	void closeConnection(int client_fd);
	void cleanup();
};

bool WebServer::_running;

#endif // HTTPSERVER_HPP
