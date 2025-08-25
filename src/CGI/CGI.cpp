/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/31 09:07:54 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/21 11:42:33 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGI.hpp"

CGI::CGI(ClientRequest &request, LocConfig *locConfig)
    : script_path_(locConfig->getFullPath()) {
	setEnv("SCRIPT_FILENAME", locConfig->getFullPath());
	setEnv("SCRIPT_NAME", "/" + request.path);
	setEnv("REQUEST_METHOD", request.method);
	setEnv("QUERY_STRING", request.query);
	if (request.extension == ".php")
		setEnv("PHPRC", locConfig->getFullPath().substr(0, locConfig->getFullPath().size() - 11));
	if (request.method == "POST") {
		setEnv("CONTENT_TYPE", request.headers["content-type"]);
		setEnv("CONTENT_LENGTH", request.headers["content-length"]);
	}
	if (request.method == "POST" || request.method == "DELETE") {
		setEnv("UPLOAD_DIR", locConfig->getUploadPath());
	}
	setEnv("SERVER_SOFTWARE", "CustomCGI/1.0");
	setEnv("GATEWAY_INTERFACE", "CGI/1.1");
	setEnv("REDIRECT_STATUS", "200");
	std::string interpreter = locConfig->getInterpreter(request.extension);
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
