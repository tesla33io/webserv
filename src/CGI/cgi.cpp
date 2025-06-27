/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:49:38 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/27 16:11:56 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./cgi.hpp"

std::string select_method(RequestMethod &method) {
	if (method == GET)
		return ("GET");
	else if (method == POST)
		return ("POST");
	else if (method == DELETE_)
		return ("DELETE");
}

std::string get_interpreter(std::string &path) {
	Logger logger;
	std::ifstream file(path);
	if (!file.is_open()) {
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid cgi file");
		return ("");
	}
	std::string first_line;
	if (std::getline(file, first_line)) {
		file.close();
		if (first_line[0] == '!' && first_line[1] == '#')
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

bool handle_CGI_request(ClientRequest &request) {
    // 1. Determine script path and interpreter
    std::string scriptPath = "www" + request.uri;
    std::string interpreter = get_interpreter(request.uri); // "/usr/bin/php"
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
        
        char* argv[] = {(char*)interpreter.c_str(), (char*)scriptPath.c_str(), NULL};
        execve(interpreter.c_str(), argv, envp);
        exit(1);
    }
    
    // 5. Parent process - send POST data if any
    close(input_pipe[0]);
    close(output_pipe[1]);
	env.free_envp(envp);
    
    if (!request.body.empty()) {
        write(input_pipe[1], request.body.c_str(), request.body.length());
    }
    close(input_pipe[1]);
    
    // 6. Read response from CGI script
    std::string response;
    char buffer[4096];
    while (read(output_pipe[0], buffer, sizeof(buffer)) > 0) {
        response += buffer;
    }
    
    // 7. Send response to client
    //sendHTTPResponse(response);
    
    // 8. Clean up
    waitpid(pid, NULL, 0);
    close(output_pipe[0]);
}