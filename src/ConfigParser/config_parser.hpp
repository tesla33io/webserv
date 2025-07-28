#ifndef CONFIG__PARSER_HPP
#define CONFIG__PARSER_HPP

#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

class ConfigNode {

  public:
	std::string name_;
	std::vector<std::string> args_;
	std::vector<ConfigNode> children_;
	int line_;

	ConfigNode()
	    : line_(0) {}
	ConfigNode(const std::string &name, std::vector<std::string> args, int line)
	    : name_(name),
	      args_(args),
	      line_(line) {}
};

class LocConfig {

  public:
	std::string path;
	std::vector<std::string> allowed_methods; // HTTP methods allowed
	int return_code;                          // HTTP redirection status (0 = no redirect)
	std::string return_target;                // HTTP redirection target
	std::string root;
	std::string alias;
	bool autoindex;                                    // directory listing
	std::string index;                                 // default files for directories
	std::string upload_path;                           // file upload directory
	std::map<std::string, std::string> cgi_extensions; // .py -> /usr/bin/python

	LocConfig()
	    : return_code(0),
	      autoindex(false) {}

	bool hasReturn() const { return return_code != 0; }

	bool hasMethod(const std::string &method) const {
		for (std::vector<std::string>::const_iterator it = allowed_methods.begin();
		     it != allowed_methods.end(); ++it) {
			if (*it == method)
				return true;
		}
		return false;
	}
};

class ServerConfig {

  public:
	std::string host;
	int port;
	std::map<int, std::string> error_pages;
	size_t client_max_body_size;
	std::vector<LocConfig> locations;
	int server_fd;

	ServerConfig()
	    : host("0.0.0.0"),
	      port(8080),
	      client_max_body_size(0) {}

	bool hasErrorPage(int status) const { return error_pages.find(status) != error_pages.end(); }

	std::string getErrorPage(int status) const {
		std::map<int, std::string>::const_iterator it = error_pages.find(status);
		return (it != error_pages.end()) ? it->second : "";
	}
	// Find by server_fd
	static ServerConfig *find(std::vector<ServerConfig> &servers, int server_fd) {
		for (std::vector<ServerConfig>::iterator it = servers.begin(); it != servers.end(); ++it) {
			if (it->server_fd == server_fd) {
				return &(*it);
			}
		}
		return NULL;
	}

	// Find by host and port
	static ServerConfig *find(std::vector<ServerConfig> &servers, const std::string &host,
	                          int port) {
		for (std::vector<ServerConfig>::iterator it = servers.begin(); it != servers.end(); ++it) {
			if (it->host == host && it->port == port) {
				return &(*it);
			}
		}
		return NULL;
	}

	// Const versions for const containers
	static const ServerConfig *find(const std::vector<ServerConfig> &servers, int server_fd) {
		for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end();
		     ++it) {
			if (it->server_fd == server_fd) {
				return &(*it);
			}
		}
		return NULL;
	}

	static const ServerConfig *find(const std::vector<ServerConfig> &servers,
	                                const std::string &host, int port) {
		for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end();
		     ++it) {
			if (it->host == host && it->port == port) {
				return &(*it);
			}
		}
		return NULL;
	}
};

class ConfigParser {

  public:
	ConfigParser();

	typedef bool (ConfigParser::*ValidationFunction)(
	    const ConfigNode &); // validation function pointer forward declaration

	struct Validity {
		std::string name_;
		std::vector<std::string> contexts_;
		bool repeatOK_;
		size_t minArgs_;
		size_t maxArgs_;
		ValidationFunction validF_;

		Validity(const std::string &n, const std::vector<std::string> &c, bool rep, size_t min,
		         size_t max, ValidationFunction f)
		    : name_(n),
		      contexts_(c),
		      repeatOK_(rep),
		      minArgs_(min),
		      maxArgs_(max),
		      validF_(f) {}
	};

	// PARSING THE CONFIGURATION FILE
	bool loadConfig(const std::string &filePath, std::vector<ServerConfig> &servers);

  private:
	Logger logg_;
	std::vector<Validity> validDirectives_;

	// Core parsing methods - tree
	bool parseTree(const std::string &filePath, ConfigNode &root);
	bool parseTreeBlocks(std::ifstream &file, int &line_nb, ConfigNode &parent);
	void processServerBlock(const ConfigNode &serverNode, ServerConfig &server);
	void processLocationBlock(const ConfigNode &locNode, LocConfig &location);
	std::string preProcess(const std::string &line) const;

	// utils for the tree
	std::vector<std::string> tokenize(const std::string &line) const;
	bool isBlockStart(const std::string &line) const;
	bool isBlockEnd(const std::string &line) const;
	bool isDirective(const std::string &line) const;

	// tree to Struct
	void convertTreeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers);
	void serverStructure(const ConfigNode &tree, std::vector<ServerConfig> &servers);
	void treeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers);

	// utils for the struct
	void handleListen(const ConfigNode &node, ServerConfig &server);
	void handleErrorPage(const ConfigNode &node, ServerConfig &server);
	void handleBodySize(const ConfigNode &node, ServerConfig &server);
	void handleLocationBlock(const ConfigNode &locNode, LocConfig &location);
	void handleReturn(const ConfigNode &node, LocConfig &location);
	void handleCGI(const ConfigNode &node, LocConfig &location);
	void handleForInherit(const ConfigNode &node, LocConfig &location);
	void inheritGeneralConfig(ServerConfig &server, const LocConfig &forInheritance);
	void sortLocations(std::vector<LocConfig> &locations);
	static bool compareLocationPaths(const LocConfig &a, const LocConfig &b);
	bool isDuplicateServer(const std::vector<ServerConfig> &servers, const ServerConfig &newServer);

	// Validation methods
	bool validateDirective(const ConfigNode &node, const ConfigNode &parent);
	bool validateListen(const ConfigNode &node);
	bool validateError(const ConfigNode &node);
	bool validateReturn(const ConfigNode &node);
	bool validateMethod(const ConfigNode &node);
	bool validateMaxBody(const ConfigNode &node);
	bool validateAutoIndex(const ConfigNode &node);
	bool validateLocation(const ConfigNode &node);
	bool validateCGI(const ConfigNode &node);
	bool validateChunk(const ConfigNode &node);
	bool validateUploadPath(const ConfigNode &node);
	bool validateRoot(const ConfigNode &node);
	bool validateAlias(const ConfigNode &node);
	bool validateIndex(const ConfigNode &node);

	// utils for validity
	void initValidDirectives();
	std::vector<std::string> makeVector(const std::string &a, const std::string &b) const;
	bool isValidIPv4(const std::string &ip);
	bool isValidUri(const std::string &uri);
	bool isValidUrl(const std::string &url);
	bool isValidPath(const std::string &path);

	// Debug print methods
	void printServers(const std::vector<ServerConfig> &servers, std::ostream &os) const;
	void printServerConfig(const ServerConfig &server, std::ostream &os) const;
	void printLocationConfig(const LocConfig &loc, std::ostream &os) const;
	void printTree(const ConfigNode &node, const std::string &prefix, bool isLast,
	               std::ostream &os) const;
	std::string joinArgs(const std::vector<std::string> &args) const;
};

#endif
