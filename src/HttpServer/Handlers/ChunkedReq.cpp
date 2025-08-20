/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ChunkedReq.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: Invalid date        by                   #+#    #+#             */
/*   Updated: 2025/08/20 22:50:48 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

bool WebServer::processChunkSize(Connection *conn) {
	size_t crlf_pos = findCRLF(conn->read_buffer);
	if (crlf_pos == std::string::npos) {
		// Need more data to read chunk size
		return false;
	}

	std::string chunk_size_line = conn->read_buffer.substr(0, crlf_pos);

	conn->read_buffer = conn->read_buffer.substr(crlf_pos + 2);

	// ignore chunk extensions after ';'
	size_t semicolon_pos = chunk_size_line.find(';');
	if (semicolon_pos != std::string::npos) {
		chunk_size_line = chunk_size_line.substr(0, semicolon_pos);
	}

	chunk_size_line = su::trim(chunk_size_line);

	conn->chunk_size = static_cast<size_t>(std::strtol(chunk_size_line.c_str(), NULL, 16));
	conn->chunk_bytes_read = 0;

	_lggr.debug("Chunk size: " + su::to_string(conn->chunk_size));


	// MAX BODY SIZE - vs CHUNKDATA + new chunk size
	if (!conn->locConfig->infiniteBodySize() && conn->locConfig->getMaxBodySize() > 0) {
		size_t total_body_size = conn->chunk_data.length() + conn->chunk_size;
		if (static_cast<size_t>(total_body_size) > conn->locConfig->getMaxBodySize()) {
			_lggr.error("Chunked body size (" + su::to_string(total_body_size) + 
			           ") would exceed max body size (" + su::to_string(conn->locConfig->getMaxBodySize()) + ")");
			prepareResponse(conn, Response(413, conn)); // Request Entity Too Large
			conn->should_close = true;
			conn->state = Connection::REQUEST_COMPLETE;
			return true;
		}
	}


	if (conn->chunk_size == 0) {
		// Last chunk, read trailers
		conn->state = Connection::READING_TRAILER;
		return processTrailer(conn);
	} else {
		conn->state = Connection::READING_CHUNK_DATA;
		return processChunkData(conn);
	}
}

bool WebServer::processChunkData(Connection *conn) {
	size_t available_data = conn->read_buffer.length();
	size_t bytes_needed = conn->chunk_size - conn->chunk_bytes_read;

	if (available_data < bytes_needed + 2) { // +2 for trailing CRLF
		// Need more data
		return false;
	}

	size_t bytes_to_read = bytes_needed;
	std::string chunk_part = conn->read_buffer.substr(0, bytes_to_read);

	conn->chunk_data += chunk_part;
	conn->chunk_bytes_read += bytes_to_read;

	conn->read_buffer = conn->read_buffer.substr(bytes_to_read);

	// Check if there are trailing CRLF
	if ((conn->read_buffer.length() < 2) || (conn->read_buffer.substr(0, 2) != "\r\n")) {
		_lggr.error("Invalid chunk format: no trailing CRLF");
		prepareResponse(conn, Response(400, conn)); // Bad Request
		conn->should_close = true;
		conn->state = Connection::REQUEST_COMPLETE;
		return true;
	}

	// Remove trailing CRLF
	conn->read_buffer = conn->read_buffer.substr(2);

	conn->state = Connection::READING_CHUNK_SIZE;
	return processChunkSize(conn);
}

bool WebServer::processTrailer(Connection *conn) {
	size_t trailer_end = findCRLF(conn->read_buffer);

	if (trailer_end == std::string::npos) {
		// Need more data
		return false;
	}

	std::string trailer_line = conn->read_buffer.substr(0, trailer_end);
	conn->read_buffer = conn->read_buffer.substr(trailer_end + 2);

	// If trailer line is empty, we're done
	if (trailer_line.empty()) {
		conn->state = Connection::CHUNK_COMPLETE;

		reconstructChunkedRequest(conn);
		return true;
	}

	return processTrailer(conn);
}


void WebServer::reconstructChunkedRequest(Connection *conn) {
	std::string reconstructed_request = conn->headers_buffer;

	std::string headers_lower = su::to_lower(reconstructed_request);

	size_t te_pos = headers_lower.find("transfer-encoding: chunked");
	if (te_pos != std::string::npos) {
		// Find the end of this header line
		size_t line_end = reconstructed_request.find("\r\n", te_pos);
		if (line_end != std::string::npos) {
			// Remove the Transfer-Encoding line
			reconstructed_request.erase(te_pos, line_end - te_pos + 2);
		}
	}

	// Final check: total reconstructed body is < MaxBody
	if (!conn->locConfig->infiniteBodySize() && conn->locConfig->getMaxBodySize() > 0) {
		if (static_cast<size_t>(conn->chunk_data.length()) > conn->locConfig->getMaxBodySize()) {
			_lggr.error("Final chunked body size (" + su::to_string(conn->chunk_data.length()) + 
			           ") exceeds max body size (" + su::to_string(conn->locConfig->getMaxBodySize()) + ")");
			prepareResponse(conn, Response(400, conn)); // Bad Request - should have been caught earlier
			conn->should_close = true;
			conn->state = Connection::REQUEST_COMPLETE;
			return;
		}
	}
	
	// Add Content-Length header before the final CRLF
	size_t final_crlf = reconstructed_request.find("\r\n\r\n");
	if (final_crlf != std::string::npos) {
		std::string content_length_header =
		    "\r\nContent-Length: " + su::to_string(conn->chunk_data.length()) + "\r\n";
		reconstructed_request.insert(final_crlf, content_length_header);
	}

	conn->read_buffer = reconstructed_request + conn->chunk_data;
	conn->state = Connection::REQUEST_COMPLETE;

	_lggr.debug("Reconstructed chunked request, total body size: " +
	            su::to_string(conn->chunk_data.length()));
}
