#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "../Logger/Logger.hpp"
#include "../RequestParser/request_parser.hpp"
#include "../Utils/StringUtils.hpp"

#include <arpa/inet.h>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

struct Request {
	std::string method;
	std::string path;
	int clfd;
};
	
#define KEEP_ALIVE_TO 5 // seconds
#define MAX_KEEP_ALIVE_REQS 100

class WebServer {

  public:
	WebServer(int p);
	~WebServer();

	bool initialize();
	void run();

	static bool _running;

	// Connection state tracking structure
	struct ConnectionInfo {
		int clfd;
		time_t last_activity;
		std::string buffer;
		bool keep_alive;
		int request_count;

		ConnectionInfo(int socket_fd)
		    : clfd(socket_fd), last_activity(time(NULL)), keep_alive(false), request_count(0) {}
	};

  private:
	int _server_fd;
	int _epoll_fd;
	int _port;
	int _backlog;
	std::map<int, ConnectionInfo *> _connections;
	std::vector<int> _conn_to_close;
	std::string _root_path;

	static const int CONNECTION_TO = 30; // seconds
	static const int CLEANUP_INTERVAL = 5; // seconds
	static const int MAX_EVENTS = 4096;
	static const int BUFFER_SIZE = 4096;

	Logger _lggr;
	time_t _last_cleanup;

	// Connection management methods
	void handleNewConnection();
	void addConnection(int client_fd);
	void updateConnectionActivity(int client_fd);
	void cleanupExpiredConnections();
	void closeConnection(int client_fd);
	bool shouldKeepAlive(const ClientRequest &req);
	void handleConnectionTimeout(int client_fd);

	// Request processing methods
	void handleClientData(int client_fd);
	bool isCompleteRequest(const std::string &request);
	void processRequest(int client_fd, const std::string &raw_req);
	void sendResponse(const ClientRequest &req);
	std::string handleGetRequest(const std::string &path);

	// Utility methods
	bool setNonBlocking(int fd);
	time_t getCurrentTime() const;
	bool isConnectionExpired(const ConnectionInfo *conn) const;
	void logConnectionStats();
	void cleanup();

  public:
	// Handlers
	std::string getFileContent(std::string path);

  private:
	// Handlers
	std::string detectContentType(const std::string &path);
	std::string generateErrorResponse(int errorCode);
};

#endif // HTTPSERVER_HPP
