/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ChunkedReq.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: Invalid date        by                   #+#    #+#             */
/*   Updated: 2025/08/25 19:12:29 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

bool WebServer::processChunkSize(Connection *conn) {
	_lggr.debug("In processChunkSize");
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

	// VALIDATION chunk size valid?
	char *end;
	errno = 0;
	long size = std::strtol(chunk_size_line.c_str(), &end, 16);
	// strtol returns 0 and sets endptr == str on error
	if (end == chunk_size_line.c_str() || errno == ERANGE || size < 0) {
		// invalid chunk size
		_lggr.error("Invalid chunk size: " + su::to_string(conn->chunk_size));
		prepareResponse(conn, Response(400, conn));
		conn->should_close = true;
		conn->state = Connection::REQUEST_COMPLETE;
		return true; // Request is "complete" with error response
	}
	
	conn->chunk_size = static_cast<size_t>(size);
	_lggr.debug("Chunk size: " + su::to_string(conn->chunk_size));

	conn->chunk_bytes_read = 0;
	

	// MAX BODY SIZE - vs CHUNKDATA + new chunk size
	if (!conn->locConfig->infiniteBodySize() && conn->locConfig->getMaxBodySize() > 0) {
		size_t total_body_size = conn->chunk_data.length() + conn->chunk_size;
		_lggr.debug("Chunk_data.length +  next chunk: " + su::to_string(total_body_size));

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
		_lggr.debug("Not enough data available, waiting for more");
		return false;
	}

	size_t bytes_to_read = bytes_needed;
	std::string chunk_part = conn->read_buffer.substr(0, bytes_to_read);

	conn->chunk_data += chunk_part;
	conn->chunk_bytes_read += bytes_to_read;

	conn->read_buffer = conn->read_buffer.substr(bytes_to_read);

	// Exactly the announced number of bytes 
	if (conn->chunk_bytes_read != conn->chunk_size) {
		_lggr.error("Chunk data length mismatch: expected " + su::to_string(conn->chunk_size) + 
		           " bytes, but read " + su::to_string(conn->chunk_bytes_read) + " bytes");
		prepareResponse(conn, Response(400, conn));
		conn->should_close = true;
		conn->state = Connection::REQUEST_COMPLETE;
		return true;
	}

	// Check if there are trailing CRLF - check we should return true
	if ((conn->read_buffer.length() < 2) || (conn->read_buffer.substr(0, 2) != "\r\n")) {
		_lggr.error("Invalid chunk format: no trailing CRLF");
		prepareResponse(conn, Response(400, conn)); // Bad Request
		conn->should_close = true;
		conn->state = Connection::REQUEST_COMPLETE;
		return true;
	}

	// Remove trailing CRLF
	conn->read_buffer = conn->read_buffer.substr(2);
	_lggr.debug("Chunk data processed successfully: " + su::to_string(conn->chunk_size) + " bytes");

	conn->chunk_bytes_read = 0;
	return processChunkSize(conn);
}

bool WebServer::processTrailer(Connection *conn) {
	size_t trailer_end = findCRLF(conn->read_buffer);

	// Need more data
	if (trailer_end == std::string::npos) {
		return false;
	}

	std::string trailer_line = conn->read_buffer.substr(0, trailer_end);
	conn->read_buffer = conn->read_buffer.substr(trailer_end + 2);

	// If trailer line is empty, we're done
	if (trailer_line.empty()) {
		conn->state = Connection::CHUNK_COMPLETE;
		_lggr.debug("Trailer line is empty, chunk complete");
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
			_lggr.debug("Removed Transfer-Encoding header from reconstruction");
		}
	}

	// Final check: total reconstructed body is < MaxBody
	_lggr.debug("Final chunked body size (" + su::to_string(conn->chunk_data.length()) + 
			") vs max body size (" + su::to_string(conn->locConfig->getMaxBodySize()) + ")");
	if (!conn->locConfig->infiniteBodySize() && conn->locConfig->getMaxBodySize() > 0) {
		if (static_cast<size_t>(conn->chunk_data.length()) > conn->locConfig->getMaxBodySize()) {
			_lggr.error("Final chunked body size (" + su::to_string(conn->chunk_data.length()) + 
					") exceeds max body size (" + su::to_string(conn->locConfig->getMaxBodySize()) + ")");
			prepareResponse(conn, Response(413, conn)); // 413 Request Entity Too Large
			conn->should_close = true;
			conn->state = Connection::REQUEST_COMPLETE;
			return ;
		}
	}

	// Add Content-Length header before the final CRLF
	size_t final_crlf = reconstructed_request.find("\r\n\r\n");
	if (final_crlf != std::string::npos) {
		std::string content_length_header =
			"\r\nContent-Length: " + su::to_string(conn->chunk_data.length()) + "\r\n";
		reconstructed_request.insert(final_crlf, content_length_header);
		_lggr.debug("Added Content-Length header: " + su::to_string(conn->chunk_data.length()));
	}

	// Store the reconstructed request but don't overwrite read_buffer yet
	// The body will be handled separately in processRequest()
	conn->read_buffer = reconstructed_request + conn->chunk_data;

	_lggr.debug("Chunked request reconstruction completed successfully");
	_lggr.debug("Reconstructed request, total body size: " +
				su::to_string(conn->chunk_data.length()));
	
	// Debug: show first part of reconstructed request
	std::string debug_preview = conn->read_buffer.substr(0, std::min(size_t(200), conn->read_buffer.size()));
	_lggr.debug("Reconstructed request preview: " + debug_preview);
}

