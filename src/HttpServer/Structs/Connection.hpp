/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:38:20 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:29:28 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "includes/Webserv.hpp"
#include "src/ConfigParser/ConfigParser.hpp"
#include "Response.hpp"

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

  public:
	ServerConfig *getServerConfig() const { return servConfig; }
};

#endif
