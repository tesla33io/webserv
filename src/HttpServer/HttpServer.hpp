#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"
#include "../RequestParser/request_parser.hpp"

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

struct Request {
	std::string method;
	std::string path;
	int clfd;
};

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
	std::string _root_path;
	Logger _lggr;

	static const int MAX_EVENTS = 64;
	static const int BUFFER_SIZE = 4096;

	bool setNonBlocking(int fd);
	void handleNewConnection();
	void handleClientData(int client_fd);
	bool isCompleteRequest(const std::string &request);
	std::string handleGetRequest(const std::string &path);
	void processRequest(int client_fd, const std::string &raw_req);
	void sendResponse(const ClientRequest &req);
	void closeConnection(int client_fd);
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
