#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "../ConfigParser/config_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../RequestParser/request_parser.hpp"
#include "../Utils/StringUtils.hpp"

#include <arpa/inet.h>
#include <climits>
#include <cstddef>
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
#include <stdint.h>
#include <string>
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

#define __WEBSERV_VERSION__ "whateverX 0.whatever.7 -- made with <3 at 42 Berlin"

class WebServer {

  public:
	WebServer(std::vector<ServerConfig> &confs);
	WebServer(std::vector<ServerConfig> &confs, std::string &prefix_path);
	~WebServer();

	bool initialize();
	void run();

	static bool _running;

	struct Response {
		std::string version;                        // HTTP/1.1
		uint16_t status_code;                       // e.g. 200
		std::string reason_phrase;                  // e.g. OK
		std::map<std::string, std::string> headers; // e.g. Content-Type: text/html
		std::string body;                           // e.g. <h1>Hello world!</h1>

		Response();
		explicit Response(uint16_t code);
		Response(uint16_t code, const std::string &response_body);

		inline void setStatus(uint16_t code);
		inline void setHeader(const std::string &name, const std::string &value);
		inline void setContentType(const std::string &ctype);
		inline void setContentLength(size_t length);
		std::string toString() const;
        std::string toShortString() const;

		// Factory methods for common responses
		static Response continue_();
		static Response ok(const std::string &body = "");
		static Response notFound();
		static Response internalServerError();
		static Response badRequest();
		static Response methodNotAllowed();

	  private:
		std::string getReasonPhrase(uint16_t code) const;
		void initFromStatusCode(uint16_t code);
	};

	// Connection state tracking structure
	struct Connection {
		int clfd;
		std::string host;
		int port;
		time_t last_activity;
		std::string read_buffer;
		Response response;
		bool response_ready;
		bool chunked;
		bool keep_alive;
		int request_count;

		// Chunked transfer state
		enum State {
			READING_HEADERS,
			REQUEST_COMPLETE,

			// chunked stuff
			READING_CHUNK_SIZE,
			READING_CHUNK_DATA,
			READING_CHUNK_TRAILER,
			READING_TRAILER,
			CHUNK_COMPLETE
		};

		State state;

		Connection(int socket_fd);
		void updateActivity();
		bool isExpired(time_t current_time, int timeout) const;
		void resetChunkedState();
		std::string toString();
	};

  private:
	int _epoll_fd;
	int _backlog;
	std::string _root_prefix_path;

	std::vector<ServerConfig> _confs;
	std::vector<ServerConfig> _have_pending_conn;

	static const int CONNECTION_TO = 30;   // seconds
	static const int CLEANUP_INTERVAL = 5; // seconds
	static const int BUFFER_SIZE = 4096 * 3;

	Logger _lggr;
	static std::map<uint16_t, std::string> err_messages;

	// Connection management arguments
	std::map<int, Connection *> _connections;
	std::vector<int> _conn_to_close;
	time_t _last_cleanup;

	// Initialization
	bool setupSignalHandlers();
	bool createEpollInstance();
	bool resolveAddress(const ServerConfig &config, struct addrinfo **result);
	bool createAndConfigureSocket(ServerConfig &config, const struct addrinfo *addr_info);
	bool setSocketOptions(int socket_fd, const std::string &host, const int port);
	bool setNonBlocking(int fd);
	bool bindAndListen(const ServerConfig &config, const struct addrinfo *addr_info);
	bool epollManage(int op, int socket_fd, uint32_t events);
	bool initializeSingleServer(ServerConfig &config);

	// Main loop
	void processEpollEvents(const struct epoll_event *events, int event_count);
	void handleClientEvent(int fd, uint32_t event_mask);

	// Connection management methods
	void handleNewConnection(ServerConfig *sc);
	Connection *addConnection(int client_fd, std::string host, int port);
	void updateConnectionActivity(int client_fd);
	void cleanupExpiredConnections();
	void closeConnection(Connection *conn);
	bool shouldKeepAlive(const ClientRequest &req);
	void handleConnectionTimeout(int client_fd);

	// Request processing methods
	void handleClientRecv(Connection *conn);
	ssize_t receiveData(int client_fd, char *buffer, size_t buffer_size);
	bool processReceivedData(Connection *conn, const char *buffer, ssize_t bytes_read,
	                         ssize_t total_bytes_read);
	void handleClientDisconnection(Connection *conn);
	void handleRequestTooLarge(Connection *conn, ssize_t bytes_read);
	bool handleCompleteRequest(int client_fd, Connection *conn);
	bool isCompleteRequest(Connection *conn);
	void processRequest(Connection *conn);
	bool parseRequest(Connection *conn, ClientRequest &req);
	ssize_t prepareResponse(Connection *conn, const Response &resp);
	bool sendResponse(Connection *conn);
	std::string handleGetRequest(const std::string &path);

	// HTTP request handlers
	Response handleGetRequest(ClientRequest &req);
	Response handlePostRequest(const ClientRequest &req);   // TODO: Implement
	Response handleDeleteRequest(const ClientRequest &req); // TODO: Implement

	// File serving methods
	std::string getFileContent(std::string path);
	std::string detectContentType(const std::string &path);
	// bool isValidPath(const std::string &path) const; -- TODO: maybe Implement

	// Error handling
	Response createErrorResponse(uint16_t code) const; // TODO: Implement

	// Utility methods
	Connection *getConnection(int client_fd);
	bool isServerSocket(int fd) const;
	void findPendingConnections(int fd);
	static void initErrMessages();
	time_t getCurrentTime() const;
	bool isConnectionExpired(const Connection *conn) const;
	void logConnectionStats();
	void cleanup();
};

// Utility functions

LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations);
bool isDirectory(const char *path);
bool isRegularFile(const char *path);

#endif // HTTPSERVER_HPP
