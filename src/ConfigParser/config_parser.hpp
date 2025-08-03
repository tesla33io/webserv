
#ifndef CONFIG__PARSER_HPP
#define CONFIG__PARSER_HPP

#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
# include <stdint.h>

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

extern const int http_status_codes[];

class ConfigNode;
class ServerConfig;
class LocConfig;
class WebServer;

// TODO: think about the interpreter path rules + upload rules, esp in the context of inheritance

class ConfigParser {
	
	
	public:
	
		ConfigParser();
		
		typedef bool (ConfigParser::*ValidationFunction)(const ConfigNode&) ; // validation function pointer forward declaration
		
		struct Validity {
			std::string name_;
			std::vector<std::string> contexts_;
			bool repeatOK_;
			size_t minArgs_;
			size_t maxArgs_;
			ValidationFunction validF_;

			Validity(	const std::string& n, 
						const std::vector<std::string>& c, 
						bool rep, 
						size_t min, 
						size_t max,
						ValidationFunction f)
						: 	name_(n), 
							contexts_(c), 
							repeatOK_(rep), 
							minArgs_(min), 
							maxArgs_(max), 
							validF_(f) 
							{}
		};

		// PARSING THE CONFIGURATION FILE
		bool loadConfig(const std::string& filePath, std::vector<ServerConfig>& servers);
	
		
	private:
		
		Logger logg_; 
		std::vector<Validity> validDirectives_;

		// Core parsing methods - tree
		bool parseTree(const std::string& filePath, ConfigNode& root);
		bool parseTreeBlocks(std::ifstream &file, int &line_nb, ConfigNode &parent);
		void processServerBlock(const ConfigNode& serverNode, ServerConfig& server);
		void processLocationBlock(const ConfigNode& locNode, LocConfig& location);
		std::string preProcess(const std::string& line) const;
		
		// utils for the tree
		std::vector<std::string> tokenize(const std::string& line) const;
		bool isBlockStart(const std::string& line) const;
		bool isBlockEnd(const std::string& line) const;
		bool isDirective(const std::string& line) const;
		
		// tree to Struct
		bool convertTreeToStruct(const ConfigNode& tree, std::vector<ServerConfig>& servers);
		void serverStructure(const ConfigNode &tree, std::vector<ServerConfig> &servers) ;
		void treeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers) ;

		// utils for the struct
		void handleListen(const ConfigNode& node, ServerConfig& server);
		void handleRoot(const ConfigNode &node, LocConfig &location);
		void handleIndex(const ConfigNode &node, LocConfig &location);
		void handleErrorPage(const ConfigNode& node, ServerConfig& server);
		void handleBodySize(const ConfigNode& node, ServerConfig& server);
		void handleLocationBlock(const ConfigNode &locNode, LocConfig &location);
		void handleReturn(const ConfigNode &node, LocConfig &location) ;
		void handleCGI(const ConfigNode &node, LocConfig &location) ;
		void handleForInherit(const ConfigNode &node, LocConfig &location);
		void inheritGeneralConfig(ServerConfig& server, const LocConfig& forInheritance);
		void sortLocations(std::vector<LocConfig>& locations) ;
		static bool compareLocationPaths(const LocConfig& a, const LocConfig& b) ;
		bool isDuplicateServer(const std::vector<ServerConfig>& servers, const ServerConfig& newServer) ;
		bool existentLocationDuplicate(const ServerConfig& server, const LocConfig& location) ;
		bool baseLocation(ServerConfig& server) ;
		void addRootToErrorUri(ServerConfig& server) ;

		// Validation methods
		bool validateDirective(const ConfigNode& node, const ConfigNode& parent) ;
		bool validateListen(const ConfigNode& node) ;
		bool validateError(const ConfigNode& node) ;
		bool validateReturn(const ConfigNode& node) ;
		bool validateMethod(const ConfigNode& node) ;
		bool validateMaxBody(const ConfigNode& node) ;
		bool validateAutoIndex(const ConfigNode& node) ;
		bool validateLocation(const ConfigNode& node) ;
		bool validateCGI(const ConfigNode& node) ;
		bool validateChunk(const ConfigNode& node) ;
		bool validateUploadPath(const ConfigNode& node) ;
		bool validateRoot(const ConfigNode& node);
		bool validateAlias(const ConfigNode& node);
		bool validateIndex(const ConfigNode& node);

		// utils for validity
		void initValidDirectives();
		std::vector<std::string> makeVector(const std::string &a, const std::string &b) const;
		static bool isValidIPv4(const std::string &ip);
		static bool isValidUri(const std::string& str);
		static bool isHttp(const std::string& url);
		static bool hasQuotes(const std::string& str);
		static bool hasDot(const std::string& str);
		static bool hasDotDot(const std::string& str);
		static bool unknownCode(uint16_t code);
		static bool hasExtension(const std::string& filepath);



		// Debug print methods
		void printServers(const std::vector<ServerConfig> &servers, std::ostream &os) const;
		void printServerConfig(const ServerConfig &server, std::ostream &os) const;
		void printLocationConfig(const LocConfig &loc, std::ostream &os) const;
		void printTree(const ConfigNode &node, const std::string &prefix, bool isLast,
					std::ostream &os) const;
		std::string joinArgs(const std::vector<std::string> &args) const;
};


class ConfigNode {
	friend class ConfigParser;

  private:
	std::string name_;
	std::vector<std::string> args_;
	std::vector<ConfigNode> children_;
	int line_;
	
  public:
	ConfigNode()
	    : line_(0) {}
	ConfigNode(const std::string &name, std::vector<std::string> args, int line)
	    : name_(name), args_(args), line_(line) {}
};

class LocConfig {
	friend class ConfigParser;
	friend class WebServer;

  private:
	std::string path;
	std::vector<std::string> allowed_methods; // HTTP methods allowed
	uint16_t return_code;                     // HTTP redirection status (0 = no redirect)
	std::string return_target;                // HTTP redirection target
	std::string root;
	std::string alias;
	bool autoindex;                                    // directory listing
	std::string index;                                 // default files for directories
	std::string upload_path;                           // file upload directory
	std::map<std::string, std::string> cgi_extensions; // .py -> /usr/bin/python


  public:
	LocConfig()
	    : return_code(0),
	      autoindex(false) {}

	inline std::string getPath() const { return path; }
	bool hasReturn() const { return return_code != 0; }
	bool hasMethod(const std::string &method) const {
		if (allowed_methods.empty())
			return true;
		for (std::vector<std::string>::const_iterator it = allowed_methods.begin();
		     it != allowed_methods.end(); ++it) {
			if (*it == method)
				return true;
		}
		return false;
	}
	bool hasExtension(const std::string &ext) const {
		return cgi_extensions.find(ext) != cgi_extensions.end();
	}
		
	std::string getExtensionPath(const std::string &ext) const {
		std::map<std::string, std::string>::const_iterator it = cgi_extensions.find(ext);
		if (it != cgi_extensions.end()) 
			return it->second;
		else
			return "";
	}
};

class ServerConfig {
	friend class ConfigParser;
	friend class WebServer;

  private:
	std::string host;
	int port;
	std::map<uint16_t, std::string> error_pages;
	size_t client_max_body_size;
	std::vector<LocConfig> locations;

	std::string root_prefix;
	int server_fd;


	
  public:
	
	ServerConfig()
	    : host("0.0.0.0"),
	      port(8080),
	      client_max_body_size(1048576) {}

	// GETTERS
	const std::string& getHost() const { return host; }
	int getPort() const { return port; }
	const std::map<uint16_t, std::string>& getErrorPages() const { return error_pages; } ;
	size_t getMaxBodySize() const { return client_max_body_size; }
	const std::string& getRootPrefix() const { return root_prefix; } ;
	std::string getErrorPage(uint16_t status) const {
		std::map<uint16_t, std::string>::const_iterator it = error_pages.find(status);
		return (it != error_pages.end()) ? it->second : "";
	}

	bool hasErrorPage(uint16_t status) const { return error_pages.find(status) != error_pages.end(); }
	bool infiniteBodySize() const { return (client_max_body_size == 0) ? true : false;}

	// The default location
	LocConfig* defaultLocation() { 
			for (std::vector<LocConfig>::iterator it = locations.begin(); it != locations.end(); ++it) {
			if (it->getPath() == "/") {
				return &(*it);
			}
		}
		return NULL;
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

#endif
