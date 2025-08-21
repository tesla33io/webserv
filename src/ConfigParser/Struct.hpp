/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Struct.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/18 14:50:47 by htharrau          #+#    #+#             */
/*   Updated: 2025/08/21 15:45:21 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STRUCT_HPP
#define STRUCT_HPP

#include "includes/Webserv.hpp"
#include "src/Logger/Logger.hpp"
#include "src/Utils/StringUtils.hpp"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

class ConfigNode;
class ServerConfig;
class LocConfig;
class WebServer;


class LocConfig {
	friend class ConfigParser;
	friend class WebServer;

  private:
	std::string path;
	bool exact_match;
	std::string full_path;
	std::vector<std::string> allowed_methods;
	uint16_t return_code;
	std::string return_target;
	size_t client_max_body_size;
	bool body_size_set;
	std::string root;
	bool autoindex;
	std::string index;
	std::string upload_path;
	std::map<std::string, std::string> cgi_extensions;

  public:
	LocConfig()
	    : exact_match(0),
		  return_code(0),
		  client_max_body_size(1048576),
		  body_size_set(false),
		  autoindex(false)  {}

	// GETTERS & SETTERS
	std::string getPath() const;
	bool is_exact_() const;
	std::string getRoot() const;
	std::string getFullPath() const;
	std::string getUploadPath() const;
	size_t getMaxBodySize() const;
	bool infiniteBodySize() const;
	bool hasReturn() const;
	bool hasMethod(const std::string &method) const;
	std::string getAllowedMethodsString();
	bool acceptExtension(const std::string &ext) const;
	std::string getInterpreter(const std::string &ext) const;
	void setExact(bool is_exact);
	void setFullPath(const std::string &path);

};


class ServerConfig {
	friend class ConfigParser;

  private:
	std::string host;
	int port;
	std::map<uint16_t, std::string> error_pages;
	std::vector<LocConfig> locations;
	std::string prefix_;
	int server_fd;

  public:
	ServerConfig()
	    : host("0.0.0.0"),
	      port(8080)  {}

		  
	// GETTERS
	const std::string &getHost() const ;
	int getPort() const;
	int getServerFD() const;
	const std::string &getPrefix() const;
	void setServerFD(int fd);
	void setPrefix(const std::string& prefix);
	const std::map<uint16_t, std::string> &getErrorPages() const;
	bool hasErrorPage(uint16_t status) const;
	std::vector<LocConfig> &getLocations();
	std::string getErrorPage(uint16_t status) const;


	// The default location
	LocConfig *defaultLocation();
	
	// Find by server_fd
	static ServerConfig *find(std::vector<ServerConfig> &servers, int server_fd);
	// Find by host and port
	static ServerConfig *find(std::vector<ServerConfig> &servers, const std::string &host, int port) ;
	// Const versions for const containers
	static const ServerConfig *find(const std::vector<ServerConfig> &servers, int server_fd);
	static const ServerConfig *find(const std::vector<ServerConfig> &servers, const std::string &host, int port) ;

};

#endif
