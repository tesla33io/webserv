/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_handler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/31 10:18:53 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/31 11:22:53 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGI.hpp"

bool CGIUtils::run_CGI_script(ClientRequest &req, CGI &cgi) {
	Logger logger;

	// 2. Creates char **envp
	char **envp = cgi.toEnvp();
	if (!envp) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to create environment");
		return (false);
	}

	// 3. Create pipes with error checking
	int input_pipe[2], output_pipe[2];
	if (pipe(input_pipe) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to create input pipe");
		cgi.freeEnvp(envp);
		return (false);
	}

	if (pipe(output_pipe) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to create output pipe");
		close(input_pipe[0]);
		close(input_pipe[1]);
		cgi.freeEnvp(envp);
		return (false);
	}
	cgi.setOutputFd(output_pipe[0]);

	// 4. Fork and execute
	pid_t pid = fork();
	cgi.setPid(pid);
	if (pid == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to fork process");
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		cgi.freeEnvp(envp);
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
		char *argv[] = {(char *)cgi.getInterpreter(), (char *)cgi.getScriptPath(), NULL};
		execve(cgi.getInterpreter(), argv, envp);
		logger.logWithPrefix(Logger::ERROR, "CGI", "Script not executable");
		cgi.freeEnvp(envp);
		exit(1);
	}

	// 5. Parent process - close unused pipe ends first
	close(input_pipe[0]);
	close(output_pipe[1]);

	// Free environment in parent (child has its own copy after fork)
	cgi.freeEnvp(envp);

	// 6. Send POST data if any
	if (req.method == "POST") {
		logger.logWithPrefix(Logger::INFO, "CGI", "Handling POST request");
		if (!req.body.empty()) {
			size_t total_written = 0;
			while (total_written < req.body.size()) {
				ssize_t written = write(input_pipe[1], req.body.c_str() + total_written,
			                        req.body.size() - total_written);
				if (written <= 0) {
					logger.logWithPrefix(Logger::WARNING, "CGI",
				                     "Failed to write request body to CGI script");
					close(input_pipe[1]);
					return (false);
				};
				total_written += written;
			}
		}
	}
	close(input_pipe[1]);
	return (true);
}

bool CGIUtils::CGI_handler(ClientRequest &req, int clfd) {
	Logger logger;
	bool chunked = false;
	// 1. Validate and construct script path
	if (req.path.empty() || req.path.find("..") != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid or potentially unsafe path");
		return (false);
	}
	CGI cgi(req);
	if (std::strcmp(cgi.getInterpreter(), "") == 0)
		return (false);
	if (!run_CGI_script(req, cgi))
		return (false);

	// 7. Read response from CGI script
	if (chunked) {
		if (!cgi.send_chunked_resp(cgi, clfd))
			return (false);
	} else {
		if (!cgi.send_normal_resp(cgi, clfd))
			return (false);
	}

	// 8. Wait for child process and check exit status
	int status;
	if (waitpid(cgi.getPid(), &status, 0) == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Failed to wait for CGI process");
		close(cgi.getOutputFd());
		return (false);
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "CGI script failed or was terminated");
		close(cgi.getOutputFd());
		return (false);
	}
	// 9. Clean up
	close(cgi.getOutputFd());
	return (true);
}
