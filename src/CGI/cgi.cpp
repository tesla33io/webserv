/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:49:38 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/24 14:33:43 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"
#include "Env.hpp"

std::string CGIUtils::select_method(RequestMethod &method) {
	if (method == GET)
		return ("GET");
	else if (method == POST)
		return ("POST");
	else if (method == DELETE_)
		return ("DELETE");
	else
		return ("");
}

std::string CGIUtils::get_interpreter(std::string &path) {
	Logger logger;
	std::ifstream file(path.c_str());
	if (!file.is_open()) {
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid cgi file");
		return ("");
	}
	std::string first_line;
	if (std::getline(file, first_line)) {
		file.close();
		if (first_line[0] == '#' && first_line[1] == '!')
			return (first_line.substr(2));
		else {
			logger.logWithPrefix(Logger::WARNING, "CGI", "No interpreter in cgi file");
			return ("");
		}
	} else {
		logger.logWithPrefix(Logger::WARNING, "CGI", "Empty cgi file");
		return ("");
	}
}

void print_cgi_response(const std::string &cgi_output) {
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

std::string extract_type(std::string &cgi_resp) {
	std::string type;
	std::istringstream ss(cgi_resp);
	std::string line;
	std::getline(ss, line);
	std::string key = "Content-type: ";
	size_t start = line.find(key);
	if (start != std::string::npos)
		type = line.substr(start);
	else
		type = "text/html";
	return (type);
}

void send_cgi_response(std::string &cgi_output, int clfd) {
	std::string type = extract_type(cgi_output);
	WebServer::Response resp;
	resp.setStatus(200);
	resp.version = "HTTP/1.1";
	resp.setContentType(extract_type(cgi_output));
	resp.setContentLength(cgi_output.length());
	size_t header_end = cgi_output.find("\n\n");
	resp.body = cgi_output.substr(header_end);
	std::string raw_response = resp.toString();
	send(clfd, raw_response.c_str(), raw_response.length(), 0);
}

bool handle_GET_request(ClientRequest &request, int clfd) {
	
}

bool CGIUtils::handle_CGI_request(ClientRequest &request, int clfd) {
	Logger logger;

	// 1. Validate and construct script path
	if (request.path.empty() || request.path.find("..") != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid or potentially unsafe path");
		return false;
	}

	std::string script_path = std::string(std::getenv("PWD")) + "/" + request.path;
	std::string interpreter = get_interpreter(script_path);
	if (interpreter.empty()) {
		return false;
	}

	// 2. Set up environment
	Env env(request);
	char **envp = env.to_envp();
	if (!envp) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to create environment");
		return false;
	}

	// 3. Create pipes with error checking
	int input_pipe[2], output_pipe[2];
	if (pipe(input_pipe) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to create input pipe");
		env.free_envp(envp);
		return false;
	}

	if (pipe(output_pipe) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to create output pipe");
		close(input_pipe[0]);
		close(input_pipe[1]);
		env.free_envp(envp);
		return false;
	}

	// 4. Fork and execute
	pid_t pid = fork();
	if (pid == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to fork process");
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		env.free_envp(envp);
		return (false);
	}

	if (pid == 0) {
		// Child process
		if (dup2(input_pipe[0], STDIN_FILENO) == -1 || dup2(output_pipe[1], STDOUT_FILENO) == -1)
			exit(1);

		// Close unused pipe ends
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(input_pipe[0]);
		close(output_pipe[1]);

		// Execute the CGI script
		char *argv[] = {(char *)interpreter.c_str(), (char *)script_path.c_str(), NULL};
		execve(interpreter.c_str(), argv, envp);
		exit(1);
	}

	// 5. Parent process - close unused pipe ends first
	close(input_pipe[0]);
	close(output_pipe[1]);

	// Free environment in parent (child has its own copy after fork)
	env.free_envp(envp);

	// 6. Send POST data if any
	if (request.method == POST) {
		logger.logWithPrefix(Logger::INFO, "CGI", "Handling POST request");
		if (!request.body.empty()) {
			size_t total_written = 0;
			while (total_written < request.body.size()) {
				ssize_t written = write(input_pipe[1], request.body.c_str() + total_written,
			                        request.body.size() - total_written);
				if (written <= 0) {
					logger.logWithPrefix(Logger::WARNING, "CGI",
				                     "Failed to write request body to CGI script");
					return (false);
				};
				total_written += written;
			}
		}
	}
	close(input_pipe[1]);

	// 7. Read response from CGI script
	std::string cgi_output;
	char buffer[4096];
	ssize_t bytes_read;

	
	while ((bytes_read = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes_read);
	}

	if (bytes_read == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Error reading from CGI script");
		close(output_pipe[0]);
		waitpid(pid, NULL, 0);
		return (false);
	}

	// 8. Wait for child process and check exit status
	int status;
	if (waitpid(pid, &status, 0) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to wait for CGI process");
		close(output_pipe[0]);
		return (false);
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "CGI script failed or was terminated");
		close(output_pipe[0]);
		return (false);
	}

	// 9. Send response to client
	//print_cgi_response(cgi_resp);
	send_cgi_response(cgi_output, clfd);

	// 10. Clean up
	close(output_pipe[0]);
	return (true);
}

