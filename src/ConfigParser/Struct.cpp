/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Struct.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/18 14:50:47 by htharrau          #+#    #+#             */
/*   Updated: 2025/08/21 15:24:36 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "includes/Webserv.hpp"
#include "src/ConfigParser/Struct.hpp"
#include "src/Logger/Logger.hpp"


/////////////////////////
// LOCCONFIG
////////

std::string LocConfig::getPath() const { 
	return path; 
}
	
bool LocConfig::is_exact_() const { 
	return exact_match; 
}

void LocConfig::setExact(bool is_exact) {
	exact_match = is_exact; 
}

std::string LocConfig::getRoot() const { 
	return path;
}

std::string LocConfig::getFullPath() const { 
	return full_path; 
}

void LocConfig::setFullPath(const std::string &path) { 
	full_path = path; 
}

std::string LocConfig::getUploadPath() const { 
	return upload_path; 
}

size_t LocConfig::getMaxBodySize() const { 
	return client_max_body_size; 
}

bool LocConfig::infiniteBodySize() const { 
	return (client_max_body_size == 0) ? true : false; 
}

bool LocConfig::hasReturn() const { 
	return return_code != 0; 
}


bool LocConfig::hasMethod(const std::string &method) const {
	if (allowed_methods.empty())
		return true;
	for (std::vector<std::string>::const_iterator it = allowed_methods.begin();
			it != allowed_methods.end(); ++it) {
		if (*it == method)
			return true;
	}
	return false;
}

std::string LocConfig::getAllowedMethodsString() {
	std::string allowed;
	for (size_t i = 0; i < allowed_methods.size(); ++i) {
		allowed += allowed_methods[i];
		if (i != allowed_methods.size() - 1)
			allowed += ", ";
	}
	return allowed;
}

bool LocConfig::acceptExtension(const std::string &ext) const {
	if (cgi_extensions.empty())
		return false;
	std::map<std::string, std::string>::const_iterator it = cgi_extensions.find(ext);
	return (it != cgi_extensions.end());
}

std::string LocConfig::getInterpreter(const std::string &ext) const {
	std::map<std::string, std::string>::const_iterator it = cgi_extensions.find(ext);
	if (it != cgi_extensions.end())
		return it->second;
	else
		return "";
}


/////////////////////////
// ServerConfig
////////

// GETTERS

const std::string &ServerConfig::getHost() const { 
	return host; 
}

int ServerConfig::getPort() const { 
	return port; 
}

int ServerConfig::getServerFD() const { 
	return server_fd; 
}

const std::string &ServerConfig::getPrefix() const { 
	return prefix_; 
}

void ServerConfig::setServerFD(int fd) { 
	server_fd = fd; 
}

void ServerConfig::setPrefix(const std::string& prefix) { 
	prefix_ = prefix; 
}

const std::map<uint16_t, std::string> &ServerConfig::getErrorPages() const { 
	return error_pages; 
}

bool ServerConfig::hasErrorPage(uint16_t status) const { 
	return error_pages.find(status) != error_pages.end();
}

std::vector<LocConfig> &ServerConfig::getLocations() { 
	return locations; 
}

std::string ServerConfig::getErrorPage(uint16_t status) const {
	std::map<uint16_t, std::string>::const_iterator it = error_pages.find(status);
	return (it != error_pages.end()) ? it->second : "";
}


// The default location
LocConfig *ServerConfig::defaultLocation() {
	for (std::vector<LocConfig>::iterator it = locations.begin(); it != locations.end(); ++it) {
		if (it->getPath() == "/") {
			return &(*it);
		}
	}
	return NULL;
}

// Find by server_fd
ServerConfig *ServerConfig::find(std::vector<ServerConfig> &servers, int server_fd) {
	for (std::vector<ServerConfig>::iterator it = servers.begin(); it != servers.end(); ++it) {
		if (it->getServerFD() == server_fd) {
			return &(*it);
		}
	}
	return NULL;
}

// Find by host and port
ServerConfig *ServerConfig::find(std::vector<ServerConfig> &servers, const std::string &host,
							int port) {
	for (std::vector<ServerConfig>::iterator it = servers.begin(); it != servers.end(); ++it) {
		if (it->host == host && it->port == port) {
			return &(*it);
		}
	}
	return NULL;
}

// Const versions for const containers
const ServerConfig *ServerConfig::find(const std::vector<ServerConfig> &servers, int server_fd) {
	for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end();
			++it) {
		if (it->getServerFD() == server_fd) {
			return &(*it);
		}
	}
	return NULL;
}

const ServerConfig *ServerConfig::find(const std::vector<ServerConfig> &servers,
								const std::string &host, int port) {
	for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end();
			++it) {
		if (it->host == host && it->port == port) {
			return &(*it);
		}
	}
	return NULL;
}

