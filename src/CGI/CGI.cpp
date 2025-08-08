/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/31 09:07:54 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 10:36:09 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGI.hpp"

CGI::CGI(ClientRequest &request, LocConfig *locConfig)
    : script_path_(locConfig->getFullPath()) {
	setEnv("SCRIPT_FILENAME", locConfig->getFullPath());
	setEnv("SCRIPT_NAME", "/" + request.path);
	setEnv("REQUEST_METHOD", request.method);
	setEnv("QUERY_STRING", request.query);
	if (request.method == "POST") {
		setEnv("CONTENT_TYPE", request.headers["content-type"]);
		setEnv("CONTENT_LENGTH", request.headers["content-length"]);
	}
	if (request.method == "POST" || request.method == "DELETE")
		setEnv("UPLOAD_DIR", "../.." + locConfig->getUploadPath());
	setEnv("SERVER_SOFTWARE", "CustomCGI/1.0");
	setEnv("GATEWAY_INTERFACE", "CGI/1.1");
	setEnv("REDIRECT_STATUS", "200");
	std::string interpreter = locConfig->getExtensionPath(request.extension);
	setInterpreter(interpreter);
}

// Set or update an environment variable
void CGI::setEnv(const std::string &key, const std::string &value) { env_[key] = value; }

// Get an environment variable's value or empty string if not found
std::string CGI::getEnv(const std::string &key) const {
	std::map<std::string, std::string>::const_iterator it = env_.find(key);
	if (it != env_.end()) {
		return (it->second);
	}
	return ("");
}

// Remove a variable if it exists
void CGI::unsetEnv(const std::string &key) { env_.erase(key); }

// Convert to a null-terminated char** suitable for execve
char **CGI::toEnvp() const {
	char **envp = new char *[env_.size() + 1];
	size_t i = 0;

	for (std::map<std::string, std::string>::const_iterator it = env_.begin(); it != env_.end();
	     ++it, ++i) {
		std::string var = it->first + "=" + it->second;
		char *c_var = new char[var.size() + 1];
		for (size_t j = 0; j < var.size(); ++j)
			c_var[j] = var[j];
		c_var[var.size()] = '\0';
		envp[i] = c_var;
	}
	envp[env_.size()] = NULL;

	return (envp);
}

// Static helper to free envp arrays created by toEnvp()
void CGI::freeEnvp(char **envp) {
	if (!envp)
		return;
	for (size_t i = 0; envp[i] != NULL; ++i)
		delete[] envp[i];
	delete[] envp;
}

/* SETTERS / GETTERS */

void CGI::setInterpreter(std::string &interpreter) { interpreter_ = interpreter; }

const char *CGI::getInterpreter() const { return (interpreter_.c_str()); }

const char *CGI::getScriptPath() const { return (script_path_.c_str()); }

void CGI::setPid(pid_t pid) { pid_ = pid; }

pid_t CGI::getPid() const { return (pid_); }

void CGI::setOutputFd(int fd) { output_fd_ = fd; }

int CGI::getOutputFd() const { return (output_fd_); }

/* CGI HANDLER */

void CGI::printCGIResponse(const std::string &cgi_output) {
	std::istringstream response_stream(cgi_output);
	std::string line;
	bool in_body = false;

	while (std::getline(response_stream, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (!in_body && line.empty()) {
			in_body = true;
			std::cout << std::endl;
			continue;
		}

		std::cout << line << std::endl;
	}
}

std::string CGI::extractContentType(std::string &cgi_headers) {
	std::string content_type = "text/html";
	std::istringstream header_stream(cgi_headers);
	std::string line;

	while (std::getline(header_stream, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (line.find("Content-Type:") == 0 || line.find("Content-type:") == 0) {
			size_t colon_pos = line.find(':');
			if (colon_pos != std::string::npos) {
				content_type = line.substr(colon_pos + 1);
				// Trim whitespace
				size_t start = content_type.find_first_not_of(" \t");
				if (start != std::string::npos) {
					content_type = content_type.substr(start);
					return (content_type);
				}
			}
		}
	}
	return (content_type);
}

bool CGI::cleanup() {
	Logger logger;
	// 8. Wait for child process and check exit status
	int status;
	if (waitpid(getPid(), &status, 0) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to wait for CGI process");
		close(getOutputFd());
		return (false);
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "CGI script failed or was terminated");
		close(getOutputFd());
		return (false);
	}
	// 9. Clean up
	close(getOutputFd());
	return (true);
}

/* NORMAL RESPONSE */

void CGI::sendCGIResponse(std::string &cgi_output, int clfd) {
	Response resp;
	resp.setStatus(200);
	resp.version = "HTTP/1.1";
	resp.setContentType(extractContentType(cgi_output));
	resp.setContentLength(cgi_output.length());
	size_t header_end = cgi_output.find("\n\n");
	resp.body = cgi_output.substr(header_end);
	std::string raw_response = resp.toString();
	send(clfd, raw_response.c_str(), raw_response.length(), 0);
}

bool CGI::sendNormalResp(CGI &cgi, int clfd) {
	Logger logger;
	std::string cgi_output;
	char buffer[4096];
	ssize_t bytes_read;

	while ((bytes_read = read(cgi.getOutputFd(), buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes_read);
	}

	if (bytes_read == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Error reading from CGI script");
		close(cgi.getOutputFd());
		waitpid(cgi.getPid(), NULL, 0);
		return (false);
	}
	cgi.printCGIResponse(cgi_output);
	cgi.sendCGIResponse(cgi_output, clfd);
	return (true);
}

/* CHUNKED RESPONSE */

void CGI::sendChunk(int clfd, const char *data, size_t size) {
	if (size == 0) {
		const char *final_chunk = "0\r\n\r\n";
		send(clfd, final_chunk, 5, 0);
		return;
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
	const char *chunk_trailer = "\r\n";
	send(clfd, chunk_trailer, 2, 0);
}

// Extract and send CGI headers first, then start chunked body
bool CGI::sendCGIHeaders(CGI &cgi, int clfd, std::string &first_chunk,
                         std::string &remaining_data) {
	Logger logger;

	// Find the end of headers (double newline)
	size_t header_end = first_chunk.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		header_end = first_chunk.find("\n\n");
		if (header_end == std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "CGI",
			                     "First chunk does not contain complete headers");
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
	std::string content_type = cgi.extractContentType(cgi_headers);

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

bool CGI::sendChunkedResp(CGI &cgi, int clfd) {
	Logger logger;
	char buffer[CHUNK_SIZE];
	std::string accumulated_data;
	bool headers_sent = false;
	ssize_t bytes_read;

	while ((bytes_read = read(cgi.getOutputFd(), buffer, CHUNK_SIZE)) > 0) {
		if (!headers_sent) {
			// Accumulate data until we have complete headers
			accumulated_data.append(buffer, bytes_read);

			std::string remaining_body;
			if (cgi.sendCGIHeaders(cgi, clfd, accumulated_data, remaining_body)) {
				headers_sent = true;

				// Send first chunk if there's body data
				if (!remaining_body.empty()) {
					cgi.sendChunk(clfd, remaining_body.c_str(), remaining_body.size());
				}
			}
			// If headers not complete, continue reading
		} else {
			// Headers already sent, send this data as a chunk
			cgi.sendChunk(clfd, buffer, bytes_read);
		}
	}

	if (bytes_read == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Error reading from CGI script");
		close(cgi.getOutputFd());
		waitpid(cgi.getPid(), NULL, 0);
		return (false);
	}

	// Send final chunk to indicate end of response
	if (headers_sent) {
		cgi.sendChunk(clfd, NULL, 0); // This sends "0\r\n\r\n"
	}
	return (true);
}
