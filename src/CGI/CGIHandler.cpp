/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/31 10:18:53 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 10:36:27 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGI.hpp"

bool CGIUtils::runCGIScript(ClientRequest &req, CGI &cgi) {
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

	usleep(10000); // 10ms delay to let execve complete or fail

	// Check if execve failed
	int status;
	pid_t wait_result = waitpid(pid, &status, WNOHANG);
	if (wait_result > 0) {
		// Child exited immediately - execve likely failed
		logger.logWithPrefix(Logger::ERROR, "CGI", "CGI script failed to execute");
		close(input_pipe[1]);
		close(output_pipe[0]);
		return (false);
	} else if (wait_result == -1) {
		// waitpid error
		logger.logWithPrefix(Logger::ERROR, "CGI", "Error checking child process status");
		close(input_pipe[1]);
		close(output_pipe[0]);
		return (false);
	}

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

CGI *CGIUtils::createCGI(ClientRequest &req, LocConfig *locConfig) {
	Logger logger;
	// 1. Validate and construct script path
	if (req.path.empty() || req.path.find("..") != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "CGI", "Invalid or potentially unsafe path");
		return (NULL);
	}

	// Heap allocated
	CGI *cgi = new CGI(req, locConfig);
	if (!runCGIScript(req, *cgi))
		return (NULL);

	return (cgi);
}
