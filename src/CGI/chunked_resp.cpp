/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   chunked_resp.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/29 12:11:18 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/29 16:11:04 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"
#include "Env.hpp"

void CGIUtils::send_chunk(int clfd, const char *data, size_t size) {
    if (size == 0) {
        const char *final_chunk = "0\r\n\r\n";
        send(clfd, final_chunk, 5, 0);
        return ;
    }
    // Convert size to hex string
    std::stringstream hex_size;
    hex_size << std::hex << size;
    std::string chunk_header = hex_size.str() + "\r\n";
    
    // Send chunk size
    send(clfd, chunk_header.c_str(), chunk_header.length(), 0);
    
    // Send chunk data
    send(clfd, data, size, 0);
    
    // Send chunk trailer
    const char* chunk_trailer = "\r\n";
    send(clfd, chunk_trailer, 2, 0);
}

// Extract and send CGI headers first, then start chunked body
bool CGIUtils::send_cgi_headers(int clfd, std::string &first_chunk, std::string &remaining_data) {
    Logger logger;
    
    // Find the end of headers (double newline)
    size_t header_end = first_chunk.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = first_chunk.find("\n\n");
        if (header_end == std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "CGI", "First chunk does not contain complete headers");
            return (false);
        }
        header_end += 2; // Skip \n\n
    } else {
        header_end += 4; // Skip \r\n\r\n
    }
    
    // Extract headers and body
    std::string cgi_headers = first_chunk.substr(0, header_end);
    remaining_data = first_chunk.substr(header_end);
    
    // Parse CGI headers to extract content-type
    std::string content_type = CGIUtils::extract_content_type(cgi_headers);
    
    // Build HTTP response headers with chunked encoding
    Response resp;
    resp.setStatus(200);
    resp.version = "HTTP/1.1";
    resp.setContentType(content_type);
    resp.setHeader("Transfer-Encoding", "chunked");
    
    // Send headers without body (chunked body will follow)
    std::string headers_only = resp.toStringHeadersOnly();
    send(clfd, headers_only.c_str(), headers_only.length(), 0);
    
    return (true);
}

bool CGIUtils::send_chunked_resp(int reading_pipe, int clfd, pid_t pid) {
	Logger logger;
    char buffer[CHUNK_SIZE];
    std::string accumulated_data;
    bool headers_sent = false;
    ssize_t bytes_read;

    while ((bytes_read = read(reading_pipe, buffer, CHUNK_SIZE)) > 0) {
        if (!headers_sent) {
            // Accumulate data until we have complete headers
            accumulated_data.append(buffer, bytes_read);
            
            std::string remaining_body;
            if (send_cgi_headers(clfd, accumulated_data, remaining_body)) {
                headers_sent = true;
                
                // Send first chunk if there's body data
                if (!remaining_body.empty()) {
                    send_chunk(clfd, remaining_body.c_str(), remaining_body.size());
                }
            }
            // If headers not complete, continue reading
        } else {
            // Headers already sent, send this data as a chunk
            send_chunk(clfd, buffer, bytes_read);
        }
    }

	if (bytes_read == -1) {
        logger.logWithPrefix(Logger::ERROR, "CGI", "Error reading from CGI script");
        close(reading_pipe);
        waitpid(pid, NULL, 0);
        return (false);
    }

    // Send final chunk to indicate end of response
    if (headers_sent) {
        send_chunk(clfd, NULL, 0); // This sends "0\r\n\r\n"
    }
	return (true);
}
