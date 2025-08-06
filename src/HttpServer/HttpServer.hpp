#ifndef __HTTPSERVER_HPP__

#define __HTTPSERVER_HPP__

#include "../../includes/webserv.hpp"
#include "../ConfigParser/config_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../RequestParser/request_parser.hpp"
#include "../Utils/StringUtils.hpp"
#include "../Utils/GeneralUtils.hpp"

#include "Response.hpp"
#include "src/CGI/CGI.hpp"

#include <arpa/inet.h>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <dirent.h> // for directory listing
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
#include <sys/stat.h>
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

/// Represents a client connection to the web server.
///
/// This class manages the state of individual client connections including
/// connection metadata, request parsing state, response handling, and
/// keep-alive functionality.
/// Represents a client connection to the web server.
///
/// This class manages the state of individual client connections including
/// connection metadata, request parsing state, response handling, and
/// keep-alive functionality.
class Connection {
	friend class WebServer;
	
	int fd;

	ServerConfig *servConfig;
	LocConfig *locConfig; 

	time_t last_activity;
	bool keep_persistent_connection;

	std::string read_buffer;
	size_t body_bytes_read; // for client_max_body_size

	bool chunked;
	size_t chunk_size;
	size_t chunk_bytes_read;
	std::string chunk_data;
	std::string headers_buffer;

	Response response;
	bool response_ready;
	int request_count;

	/// Represents the current state of request processing.
	enum State {
		READING_HEADERS,  ///< Reading request headers
		REQUEST_COMPLETE, ///< Complete request received

		CONTINUE_SENT,         ///< 100-Continue response sent
		READING_CHUNK_SIZE,    ///< Reading chunk size line
		READING_CHUNK_DATA,    ///< Reading chunk data
		READING_CHUNK_TRAILER, ///< Reading chunk trailer
		READING_TRAILER,       ///< Reading final trailer
		CHUNK_COMPLETE         ///< Chunked transfer complete
	};

	State state;

	/// Constructs a new Connection object.
	/// \param socket_fd The file descriptor for the client socket.
	Connection(int socket_fd);

	/// Updates the last activity timestamp to the current time.
	void updateActivity();

	/// Checks if the connection has expired based on timeout.
	/// \param current_time The current timestamp.
	/// \param timeout The timeout value in seconds.
	/// \returns True if the connection has expired, false otherwise.
	bool isExpired(time_t current_time, int timeout) const;

	/// Resets the chunked transfer state to initial values.
	void resetChunkedState();

	/// Returns a string representation of the connection for debugging.
	/// \returns A formatted string containing connection details.
	std::string toString();

	/// Converts a connection state enum to its string representation.
	/// \param state The connection state to convert.
	/// \returns The string representation of the state.
	std::string stateToString(Connection::State state);

	void resetForNewRequest(); // reset locConfig body_bytes_read, ...

  public :
	ServerConfig* getServerConfig() const { return servConfig; }
};

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

	/// HttpServer.cpp

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

	/// Initializes a single server configuration.
	/// \param config The server configuration to initialize.
	/// \returns True on successful initialization, false otherwise.
	bool initializeSingleServer(ServerConfig &config);

	/// !!! DEPRECATED !!!
	/// Gets the current system time.
	/// \deprecated Use standard library functions instead.
	/// \returns Current time as time_t.
	time_t getCurrentTime() const;

	/// !!! DEPRECATED !!!
	/// Checks if a connection has expired.
	/// \deprecated Use Connection::isExpired instead.
	/// \param conn The connection to check.
	/// \returns True if expired, false otherwise.
	bool isConnectionExpired(const Connection *conn) const;

	/// !!! DEPRECATED !!!
	/// Retrieves a connection object by file descriptor.
	/// \deprecated Direct map access is preferred.
	/// \param client_fd The client file descriptor.
	/// \returns Pointer to Connection object or nullptr if not found.
	Connection *getConnection(int client_fd);

	/// Performs cleanup of all server resources and connectioqns.
	void cleanup();

	/// Connection.cpp

	/// Accepts a new client connection and adds it to the connection pool.
	/// \param sc Pointer to the server configuration that received the connection.
	void handleNewConnection(ServerConfig *sc);

	/// Creates and registers a new client connection.
	/// \param client_fd The client socket file descriptor.
	/// \param sc The configuaration struct for the matching host:port server
	/// \returns Pointer to the newly created Connection object.
	Connection *addConnection(int client_fd, ServerConfig *sc) ;

	/// !!! DEPRECATED !!!
	/// Updates the last activity time for a connection.
	/// \deprecated Use Connection::updateActivity instead.
	/// \param client_fd The client file descriptor.
	void updateConnectionActivity(int client_fd);

	/// !!! DEPRECATED !!!
	/// Determines if a connection should be kept alive based on request headers.
	/// \deprecated Logic (will be) moved to antoher place.
	/// \param req The client request to analyze.
	/// \returns True if connection should be kept alive, false otherwise.

	/// !!! DEPRECATED !!!
	/// Determines if a connection should be kept alive based on request headers.
	/// \deprecated Logic (will be) moved to antoher place.
	/// \param req The client request to analyze.
	/// \returns True if connection should be kept alive, false otherwise.
	bool shouldKeepAlive(const ClientRequest &req);

	/// Closes expired connections to free resources.
	void cleanupExpiredConnections();

	/// Handles connection timeout by preparing appropriate response.
	/// \param client_fd The file descriptor of the timed-out connection.
	void handleConnectionTimeout(int client_fd);

	/// Gracefully closes a client connection.
	/// \param conn Pointer to the connection to close.
	void closeConnection(Connection *conn);

	/// Request.cpp

	/// Handles cases where request size exceeds limits.
	/// \param conn The connection that sent the oversized request.
	/// \param bytes_read Number of bytes that were read.
	void handleRequestTooLarge(Connection *conn, ssize_t bytes_read);

	/// Processes a complete HTTP request and prepares response.
	/// \param conn The connection containing the complete request.
	/// \returns True if request was processed successfully, false otherwise.
	bool handleCompleteRequest(Connection *conn);

	/// Parses and processes an HTTP request from connection buffer.
	/// \param conn The connection containing the request data.
	void processRequest(Connection *conn);

	/// Parses HTTP request and handles parsing errors.
	/// \param conn The connection containing request data.
	/// \param req Reference to ClientRequest object to populate.
	/// \returns True if parsing succeeded, false otherwise.
	bool parseRequest(Connection *conn, ClientRequest &req);

	/// Determines if a complete HTTP request has been received.
	/// \param conn The connection to check.
	/// \returns True if request is complete, false if more data is needed.
	bool isRequestComplete(Connection *conn);

	/// Checks if complete HTTP headers have been received.
	/// \param conn The connection to check.
	/// \returns True if headers are complete, false otherwise.
	bool isHeadersComplete(Connection *conn);

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

	/// Handlers/EpollEventHandler.cpp

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

	/// Handlers/ResponseHandler.cpp

	/// Prepares response data for transmission to client.
	/// \param conn The connection to send response to.
	/// \param resp The response object containing response data.
	/// \returns Number of bytes prepared for sending, or negative on error.
	ssize_t prepareResponse(Connection *conn, const Response &resp);

	/// Sends prepared response data to client.
	/// \param conn The connection to send response to.
	/// \returns True if response was sent successfully, false otherwise.
	bool sendResponse(Connection *conn);

	// CGI
	void sendCGIResponse(std::string &cgi_output, CGI *cgi, Connection *conn);
	void normalResponse(CGI *cgi, Connection *conn);
	void chunkedResponse(CGI *cgi, Connection *conn);
	void handleCGIOutput(int fd);
	bool isCGIFd(int fd) const;
	bool handleCGIRequest(ClientRequest &req, Connection *conn);

	// HTTP request handlers
	Response handleGetRequest(ClientRequest &req);
	Response handlePostRequest(ClientRequest &req);   // TODO: Implement
	Response handleDeleteRequest(ClientRequest &req); // TODO: Implement
	// DEPRECATED
	/// Handles HTTP GET requests by serving requested resources.
	/// \param req The GET request to process.
	/// \param conn The connection to send response to.
	/// \returns Response object containing the requested resource or error.
	// Response handleGetRequest(ClientRequest &req, Connection *conn);

	/// Handles Return directives.
	/// \param req The GET request to process.
	/// \param code The status code to send back.
	/// \param target The uri or url to send.
	/// \returns Response object containing the requested resource or error.
	Response handleReturnDirective(Connection* conn, uint16_t code, std::string target);

	/// Handlers/FileHandler.cpp

	/// Reads file content from filesystem.
	/// \param path The filesystem path to the file.
	/// \returns File content as string, or empty string on error.
	std::string getFileContent(std::string path);

	/// Prepares response data for transmission of a file to client
	/// \param conn The connection to send response to.
	/// \param dir_path The full path to the file to send.
	/// \returns Response object containing the requested resource or error.
	Response handleFileRequest(Connection* conn, const std::string& dir_path);

	/// Prepares response data when a directory is requested
	/// \param conn The connection to send response to.
	/// \param dir_path The response object containing the path
	/// \returns Response object containing the requested resource or error.
	Response handleDirectoryRequest(Connection* conn, const std::string& dir_path);

	/// Prepares response data for transmission to client : directory listing
	/// \param conn The connection to send response to.
	/// \param fullDirPath The response object containing the file directory.
	/// \returns Response object containing the requested resource or error.
	Response generateDirectoryListing(Connection* conn, const std::string &fullDirPath);

	// Utility methods
	void findPendingConnections(int fd);
	static void initErrMessages();
	std::string buildFullPath(const std::string& uri, LocConfig *Location);
	void logConnectionStats();
};


// Utility functions

/// Finds the location configuration that best matches a URI.
/// \note The logic is very basic for now and needs to be improved in the future
/// \param uri The URI to match against location patterns.
/// \param locations Vector of location configurations to search.
/// \returns Pointer to the best matching LocConfig, or nullptr if no match.
LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations);

/** deprecated **/
/// Checks if the given path refers to a directory.
/// \param path The filesystem path to check.
/// \returns True if path is a directory, false otherwise.
// bool isDirectory(const char *path);
/** deprecated **/
/// Checks if the given path refers to a regular file.
/// \param path The filesystem path to check.
/// \returns True if path is a regular file, false otherwise.
// bool isRegularFile(const char *path);


/// Converts epoll event flags to human-readable string representation.
/// \param ev The epoll event flags to describe.
/// \returns String representation of the events (e.g., "EPOLLIN | EPOLLOUT").
std::string describeEpollEvents(uint32_t ev);

/// Searches for CRLF (\r\n) sequence in buffer starting from given position.
/// \param buffer The string buffer to search in.
/// \param start_pos The position to start searching from.
/// \returns Position of CRLF sequence, or string::npos if not found.
size_t findCRLF(const std::string &buffer, size_t start_pos);


/// Determines appropriate Content-Type header based on file extension.
/// \param path The file path to analyze.
/// \returns Content-Type string. Default : "application/octet-stream" (for arbitrary binary data)
std::string detectContentType(const std::string &path);

// /// Determines the extension from a file path (no . allowed in the path).
// /// \param path The file path to analyze.
// /// \returns extension string (.html, .png, ...). Not found: returns "".
std::string getExtension(const std::string& path);

enum MaxBody {
		DEFAULT,
		INFINITE,
		SPECIFIED
} ;

enum FileType {
	ISDIR,
	ISREG,
	NOT_FOUND_404,
	PERMISSION_DENIED_403,
	FILE_SYSTEM_ERROR_500
};

/// Determines the type of path: .
/// \param path The path to analyze.
/// \returns file type Directory, File, Denied access, Not found, Internal error.
FileType checkFileType(std::string path) ;

#endif  /* end of include guard: __HTTPSERVER_HPP__*/

