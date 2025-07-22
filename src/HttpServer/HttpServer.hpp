#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "../Logger/Logger.hpp"
#include "../RequestParser/request_parser.hpp"
#include "../Utils/StringUtils.hpp"
#include "../ConfigParser/config_parser.hpp"

#include <arpa/inet.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define KEEP_ALIVE_TO 5 // seconds
#define MAX_KEEP_ALIVE_REQS 100
#define MAX_EVENTS 4096

#ifndef uint16_t
#define uint16_t unsigned short
#endif // uint16_t

class WebServer {

  public:
	WebServer(int port);
	WebServer(std::string &host, int port);
	WebServer(std::vector<ServerConfig> &confs);
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

		ConnectionInfo(int socket_fd);
		void updateActivity();
		bool isExpired(time_t current_time, int timeout) const;
	};

	struct Response {
		std::string version;                        // HTTP/1.1
		uint16_t status_code;                       // e.g. 200
		std::string reason_phrase;                  // e.g. OK
		std::map<std::string, std::string> headers; // e.g. Content-Type: text/html
		std::string body;                           // e.g. <h1>Hello world!</h1>

		Response();
		explicit Response(uint16_t code);
		Response(uint16_t code, const std::string &response_body);

		void setStatus(uint16_t code);
		void setHeader(const std::string &name, const std::string &value);
		void setContentType(const std::string &ctype);
		void setContentLength(size_t length);
		std::string toString() const;

		// Factory methods for common responses
		static Response ok(const std::string &body = "");
		static Response notFound();
		static Response internalServerError();
		static Response badRequest();
		static Response methodNotAllowed();

	  private:
		std::string getReasonPhrase(uint16_t code) const;
		void initFromStatusCode(uint16_t code);
	};

  private:
	std::string _host;
	int _server_fd;
	int _epoll_fd;
	int _port;
	int _backlog;
	ssize_t _max_content_length;
	std::string _root_path;

    std::vector<ServerConfig> _confs;
    std::vector<ServerConfig> _have_pending_conn;

	static const int CONNECTION_TO = 30;   // seconds
	static const int CLEANUP_INTERVAL = 5; // seconds
	static const int BUFFER_SIZE = 4096 * 3;

	Logger _lggr;
	static std::map<uint16_t, std::string> err_messages;

	// Connection management arguments
	std::map<int, ConnectionInfo *> _connections;
	std::vector<int> _conn_to_close;
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
	ssize_t sendResponse(const int clfd, const Response &resp);
	std::string handleGetRequest(const std::string &path);

	// HTTP request handlers
	Response handleGetRequest(const ClientRequest &req);
	Response handlePostRequest(const ClientRequest &req);   // TODO: Implement
	Response handleDeleteRequest(const ClientRequest &req); // TODO: Implement

	// File serving methods
	std::string getFileContent(std::string path);
	std::string detectContentType(const std::string &path);
	// bool isValidPath(const std::string &path) const; -- TODO: maybe Implement

	// Error handling
	Response createErrorResponse(uint16_t code) const; // TODO: Implement

	// Utility methods
	static void initErrMessages();
	bool setNonBlocking(int fd);
	time_t getCurrentTime() const;
	bool isConnectionExpired(const ConnectionInfo *conn) const;
	void logConnectionStats();
	void cleanup();
};

#endif // HTTPSERVER_HPP
