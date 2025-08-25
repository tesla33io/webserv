/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:38:20 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:24:49 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "Response.hpp"
#include "includes/Types.hpp"
#include "includes/Webserv.hpp"
#include "src/ConfigParser/Struct.hpp"

class WebServer;
class Response;

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
	ssize_t content_length; // ignore if -1

	std::vector<unsigned char> body_data;

	bool chunked;
	size_t chunk_size;
	size_t chunk_bytes_read;
	std::string chunk_data;
	std::string headers_buffer;

	ClientRequest parsed_request;

	Response response;
	std::string cgi_response;
	bool response_ready;
	int request_count;
	bool should_close;

	/// Represents the current state of request processing.
	enum State {
		READING_HEADERS,  ///< Reading request headers
		REQUEST_COMPLETE, ///< Complete request received
		READING_BODY,     ///< Reading request body, when Content-Length > 0
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

  public:
	ServerConfig *getServerConfig() const { return servConfig; }
};

#endif
