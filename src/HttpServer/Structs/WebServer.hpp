/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:44:09 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:29:50 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERVER2_HPP
#define WEBSERVER2_HPP

#include "Connection.hpp"
#include "Response.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "includes/Types.hpp"

class ServerConfig; // Still needed to break potential circular dependencies
class Connection;
class CGI;

/// HTTP web server implementation using epoll for event-driven I/O.
///
/// This class implements a multi-server HTTP web server that can handle
/// multiple server configurations, persistent connections, chunked transfer
/// encoding, and provides basic HTTP request/response functionality.
class WebServer {

  public:
	/// Constructs a WebServer with the given server configurations.
	/// \param confs Vector of server configurations to initialize.
	WebServer(std::vector<ServerConfig> &confs);

	/// !!! DEPRECATED !!!
	/// Constructs a WebServer with configurations and a root path prefix.
	/// \param confs Vector of server configurations to initialize.
	/// \param prefix_path Root directory prefix for serving files.
	WebServer(std::vector<ServerConfig> &confs, std::string &prefix_path);

	~WebServer();

	/// Initializes all server sockets and prepares for accepting connections.
	/// \returns True on successful initialization, false otherwise.
	bool initialize();

	/// Starts the main event loop to handle client connections and requests.
	void run();

	/// Global flag indicating if the server should continue running.
	static bool _running;

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

	/// @brief List of all CGI Objects
	std::map<int, std::pair<CGI *, Connection *> > _cgi_pool;

	// Connection management arguments
	std::map<int, Connection *> _connections;
	time_t _last_cleanup;

	// MEMBER FUNCTIONS

	/* HttpServer.cpp */

	/// Sets up signal handlers for graceful server shutdown.
	/// \returns True on success, false on failure.
	bool setupSignalHandlers();

	/// Creates and configures the main epoll instance.
	/// \returns True on success, false on failure.
	bool createEpollInstance();

	/// Resolves network address information for a server configuration.
	/// \param config The server configuration containing host/port information.
	/// \param result Pointer to store the resolved address information.
	/// \returns True on successful resolution, false otherwise.
	bool resolveAddress(const ServerConfig &config, struct addrinfo **result);

	/// Creates a socket and configures it with appropriate options.
	/// \param config Server configuration to create socket for.
	/// \param addr_info Resolved address information for the socket.
	/// \returns True on successful creation and configuration, false otherwise.
	bool createAndConfigureSocket(ServerConfig &config, const struct addrinfo *addr_info);

	/// Sets SO_REUSEADDR socket option to allow address reuse.
	/// \param socket_fd The socket file descriptor to configure.
	/// \param host The host address for logging purposes.
	/// \param port The port number for logging purposes.
	/// \returns True on success, false on failure.
	bool setSocketOptions(int socket_fd, const std::string &host, const int port);

	/// Sets a file descriptor to non-blocking mode.
	/// \deprecated This function is no longer used.
	/// \param fd The file descriptor to set as non-blocking.
	/// \returns True on success, false on failure.
	bool setNonBlocking(int fd);

	/// Binds socket to address and starts listening for connections.
	/// \param config Server configuration containing socket information.
	/// \param addr_info Address information to bind to.
	/// \returns True on successful bind and listen, false otherwise.
	bool bindAndListen(const ServerConfig &config, const struct addrinfo *addr_info);

	/// Manages epoll events for file descriptors.
	/// \param op The epoll operation (EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL).
	/// \param socket_fd The file descriptor to manage.
	/// \param events The epoll events to set (e.g. EPOLLIN | EPOLLOUT).
	/// \returns True on success, false on failure.
	bool epollManage(int op, int socket_fd, uint32_t events);

	/// Initializes a single server configuration.
	/// \param config The server configuration to initialize.
	/// \returns True on successful initialization, false otherwise.
	bool initializeSingleServer(ServerConfig &config);

	/// Performs cleanup of all server resources and connectioqns.
	void cleanup();

	/* Request.cpp */

	bool handleCGIRequest(ClientRequest &req, Connection *conn);

	/// Handles cases where request size exceeds limits.
	/// \param conn The connection that sent the oversized request.
	/// \param bytes_read Number of bytes that were read.
	void handleRequestTooLarge(Connection *conn, ssize_t bytes_read);

	/// Processes a complete HTTP request and prepares response.
	/// \param conn The connection containing the complete request.
	/// \returns True if request was processed successfully, false otherwise.
	bool handleCompleteRequest(Connection *conn);

	/// Checks if complete HTTP headers have been received.
	/// \param conn The connection to check.
	/// \returns True if headers are complete, false otherwise.
	bool isHeadersComplete(Connection *conn);

	/// Determines if a complete HTTP request has been received.
	/// \param conn The connection to check.
	/// \returns True if request is complete, false if more data is needed.
	bool isRequestComplete(Connection *conn);

	/// Parses and processes an HTTP request from connection buffer.
	/// \param conn The connection containing the request data.
	void processRequest(Connection *conn);

	/// Parses HTTP request and handles parsing errors.
	/// \param conn The connection containing request data.
	/// \param req Reference to ClientRequest object to populate.
	/// \returns True if parsing succeeded, false otherwise.
	bool parseRequest(Connection *conn, ClientRequest &req);

	/* ServerUtils.cpp */

	/// Gets the current system time.
	/// \deprecated Use standard library functions instead.
	/// \returns Current time as time_t.
	time_t getCurrentTime() const;

	/// Reads file content from filesystem.
	/// \param path The filesystem path to the file.
	/// \returns File content as string, or empty string on error.
	std::string getFileContent(std::string path);
	FileType checkFileType(std::string path);
	std::string buildFullPath(const std::string &uri, LocConfig *Location);

	// HANDLERS

	/* Handlers/ChunkedReq.cpp */

	/// Processes chunked transfer encoding chunk size line.
	/// \param conn The connection receiving chunked data.
	/// \returns True if chunk size was successfully parsed, false otherwise.
	bool processChunkSize(Connection *conn);

	/// Processes chunked transfer encoding data chunks.
	/// \param conn The connection receiving chunked data.
	/// \returns True if chunk data was processed successfully, false otherwise.
	bool processChunkData(Connection *conn);

	/// Processes trailing headers in chunked transfer encoding.
	/// \param conn The connection receiving chunked data.
	/// \returns True if trailer was processed successfully, false otherwise.
	bool processTrailer(Connection *conn);

	/// Reconstructs a complete request from chunked transfer encoding.
	/// \param conn The connection that received chunked data.
	void reconstructChunkedRequest(Connection *conn);

	/* Handlers/ServerCGI.cpp */
	void sendCGIResponse(std::string &cgi_output, CGI *cgi, Connection *conn);
	void normalResponse(CGI *cgi, Connection *conn);
	void chunkedResponse(CGI *cgi, Connection *conn);
	void handleCGIOutput(int fd);
	bool isCGIFd(int fd) const;

	/* Handlers/Connection.cpp */

	/// Accepts a new client connection and adds it to the connection pool.
	/// \param sc Pointer to the server configuration that received the connection.
	void handleNewConnection(ServerConfig *sc);

	/// Creates and registers a new client connection.
	/// \param client_fd The client socket file descriptor.
	/// \param sc The configuaration struct for the matching host:port server
	/// \returns Pointer to the newly created Connection object.
	Connection *addConnection(int client_fd, ServerConfig *sc);

	/// Closes expired connections to free resources.
	void cleanupExpiredConnections();

	/// Handles connection timeout by preparing appropriate response.
	/// \param client_fd The file descriptor of the timed-out connection.
	void handleConnectionTimeout(int client_fd);

	/// Gracefully closes a client connection.
	/// \param conn Pointer to the connection to close.
	void closeConnection(Connection *conn);

	/* Handlers/DirectoryReq.cpp */

	/// Prepares response data when a directory is requested
	/// \param conn The connection to send response to.
	/// \param dir_path The response object containing the path
	/// \returns Response object containing the requested resource or error.
	Response handleDirectoryRequest(Connection *conn, const std::string &dir_path);

	/// Prepares response data for transmission to client : directory listing
	/// \param conn The connection to send response to.
	/// \param fullDirPath The response object containing the file directory.
	/// \returns Response object containing the requested resource or error.
	Response generateDirectoryListing(Connection *conn, const std::string &fullDirPath);

	/// Prepares response data for transmission of a file to client
	/// \param conn The connection to send response to.
	/// \param dir_path The full path to the file to send.
	/// \returns Response object containing the requested resource or error.
	Response handleFileRequest(Connection *conn, const std::string &dir_path);

	/// Handles Return directives
	/// \param req The GET request to process.
	/// \param code The status code to send back.
	/// \param target The uri or url to send.
	/// \returns Response object containing the requested resource or error.
	Response handleReturnDirective(Connection *conn, uint16_t code, std::string target);

	static std::string getExtension(const std::string &path);
	static std::string detectContentType(const std::string &path);

	/* EpollEventHandler.cpp */

	/// Processes events returned by epoll_wait.
	/// \param events Array of epoll events to process.
	/// \param event_count Number of events in the array.
	void processEpollEvents(const struct epoll_event *events, int event_count);

	/// Determines if a file descriptor belongs to a listening socket.
	/// \param fd The file descriptor to check.
	/// \returns True if fd is a listening socket, false otherwise.
	bool isListeningSocket(int fd) const;

	/// Handles epoll events for client connections.
	/// \param fd The client file descriptor.
	/// \param event_mask The epoll event mask indicating event types.
	void handleClientEvent(int fd, uint32_t event_mask);

	/// Handles receiving data from a client connection.
	/// \param conn The connection to receive data from.
	void handleClientRecv(Connection *conn);

	/// Receives data from client socket with error handling.
	/// \param client_fd The client socket file descriptor.
	/// \param buffer Buffer to store received data.
	/// \param buffer_size Size of the receive buffer.
	/// \returns Number of bytes received, or negative value on error.
	ssize_t receiveData(int client_fd, char *buffer, size_t buffer_size);

	/// Processes received data and determines if request is complete.
	/// \param conn The connection that received data.
	/// \param buffer The buffer containing received data.
	/// \param bytes_read Number of bytes received in this call.
	/// \param total_bytes_read Total bytes received for this request.
	/// \returns True if processing succeeded, false on error.
	bool processReceivedData(Connection *conn, const char *buffer, ssize_t bytes_read,
	                         ssize_t total_bytes_read);

	/* Handlers/MethodsHandler.cpp */

	Response handleGetRequest(ClientRequest &req);
	Response handlePostRequest(ClientRequest &req);   // TODO: Implement
	Response handleDeleteRequest(ClientRequest &req); // TODO: Implement

	/* Handlers/ResponseHandler.cpp */

	/// Prepares response data for transmission to client.
	/// \param conn The connection to send response to.
	/// \param resp The response object containing response data.
	/// \returns Number of bytes prepared for sending, or negative on error.
	ssize_t prepareResponse(Connection *conn, const Response &resp);

	/// Sends prepared response data to client.
	/// \param conn The connection to send response to.
	/// \returns True if response was sent successfully, false otherwise.
	bool sendResponse(Connection *conn);
};

#endif
