/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:49:38 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/03 14:34:21 by jalombar         ###   ########.fr       */
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
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid cgi file1");
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
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid cgi file");
		return ("");
	}
}

void print_cgi_response(const std::string& cgi_output) {
    std::istringstream response_stream(cgi_output);
    std::string line;
    bool in_body = false;

    while (std::getline(response_stream, line)) {
        // Trim carriage return if present (from \r\n line endings)
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (!in_body && line.empty()) {
            in_body = true;
            std::cout << std::endl; // print blank line to separate headers from body
            continue;
        }

        std::cout << line << std::endl;
    }
}

bool CGIUtils::handle_CGI_request(ClientRequest &request) {
	// 1. Determine script path and interpreter
    std::string script_path = "/home/jalombar/github/CC/webserv/CGI/" + request.path;
    std::string interpreter = get_interpreter(request.path); // "/usr/bin/php"
	if (interpreter.empty())
		return (false);
    
    // 2. Set up environment
    Env env(request);
	char **envp = env.to_envp();
    
    // 3. Create pipes
    int input_pipe[2], output_pipe[2];
    pipe(input_pipe);
    pipe(output_pipe);
    
    // 4. Fork and execute
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
		close(input_pipe[1]);
		close(output_pipe[0]);

		//char* argv[] = {(char*)interpreter.c_str(), (char*)script_path.c_str(), NULL};
		char* argv[] = {(char*)"php-cgi", (char*)script_path.c_str(), NULL};
		execve(interpreter.c_str(), argv, envp);
		env.free_envp(envp);
		exit(1);
    }
    
    // 5. Parent process - send POST data if any
    close(input_pipe[0]);
    close(output_pipe[1]);
	env.free_envp(envp);

	if (!request.body.empty()) {
    	size_t total_written = 0;
		while (total_written < request.body.size()) {
			ssize_t written = write(input_pipe[1], request.body.c_str() + total_written, request.body.size() - total_written);
			if (written <= 0) break; // handle error
			total_written += written;
		}
	}
	close(input_pipe[1]);
    
    /* if (!request.body.empty()) {
        write(input_pipe[1], request.body.c_str(), request.body.length());
    }
	close(input_pipe[1]); */
    
    // 6. Read response from CGI script
    std::string response;
    char buffer[4096];
    ssize_t bytes_read;
	while ((bytes_read = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
		response.append(buffer, bytes_read);
	}
    
    // 7. Send response to client
	print_cgi_response(response);
    //sendHTTPResponse(response);
    
    // 8. Clean up
    waitpid(pid, NULL, 0);
    close(output_pipe[0]);
	return (true);
}