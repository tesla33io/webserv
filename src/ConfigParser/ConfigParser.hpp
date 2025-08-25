/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 13:53:23 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/21 15:01:09 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG__PARSER_HPP
#define CONFIG__PARSER_HPP

#include "includes/Webserv.hpp"
#include "src/Logger/Logger.hpp"
#include "src/Utils/StringUtils.hpp"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

extern const int http_status_codes[];

class ConfigNode;
class ServerConfig;
class LocConfig;
class WebServer;

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
	    : name_(name),
	      args_(args),
	      line_(line) {}
};

class ConfigParser {

  public:
	ConfigParser(int log_level);

	typedef bool (ConfigParser::*ValidationFunction)(const ConfigNode &);

	struct Validity {
		std::string name_;
		std::vector<std::string> contexts_;
		bool repeatOK_;
		size_t min_args_;
		size_t max_args_;
		ValidationFunction valid_f_;

		Validity(const std::string &n, const std::vector<std::string> &c, bool rep, size_t min,
		         size_t max, ValidationFunction f)
		    : name_(n),
		      contexts_(c),
		      repeatOK_(rep),
		      min_args_(min),
		      max_args_(max),
		      valid_f_(f) {}
	};

	// PARSING THE CONFIGURATION FILE
	bool loadConfig(const std::string &filePath, std::vector<ServerConfig> &servers, std::string &prefix, int log_level);

  private:
	Logger logg_;
	std::vector<Validity> validDirectives_;

	// Core parsing methods - tree
	bool parseTree(const std::string &filePath, ConfigNode &root);
	bool parseTreeBlocks(std::ifstream &file, int &line_nb, ConfigNode &parent);
	std::string preProcess(const std::string &line) const;

	// utils for the tree
	std::vector<std::string> tokenize(const std::string &line) const;
	bool isBlockStart(const std::string &line) const;
	bool isBlockEnd(const std::string &line) const;
	bool isDirective(const std::string &line) const;

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
	bool validateIndex(const ConfigNode &node);

	// utils for validity
	void initValidDirectives();
	static std::vector<std::string> makeVector(const std::string &a, const std::string &b);
	static bool isValidIPv4(const std::string &ip);
	static bool isValidUri(const std::string &str);
	static bool isHttp(const std::string &url);
	static bool hasOKChar(const std::string &str);
	static bool unknownCode(uint16_t code);

	// tree to Struct
	bool convertTreeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers, std::string &prefix);

	// handles the directives for the struct
	void handleListen(const ConfigNode &node, ServerConfig &server);
	void handleErrorPage(const ConfigNode &node, ServerConfig &server);
	void handleRoot(const ConfigNode &node, LocConfig &location, const std::string &prefix);
	void handleIndex(const ConfigNode &node, LocConfig &location);
	void handleBodySize(const ConfigNode &node, LocConfig &location);
	void handleLocationBlock(const ConfigNode &locNode, LocConfig &location, const std::string &prefix);
	void handleReturn(const ConfigNode &node, LocConfig &location);
	void handleCGI(const ConfigNode &node, LocConfig &location);
	void handleForInherit(const ConfigNode &node, LocConfig &location, const std::string &prefix);
	
	//  struct validation and refinments
	void inheritGeneralConfig(ServerConfig &server, const LocConfig &forInheritance);
	void sortLocations(std::vector<LocConfig> &locations);
	static bool compareLocationPaths(const LocConfig &a, const LocConfig &b);
	bool isDuplicateServer(const std::vector<ServerConfig> &servers, const ServerConfig &newServer);
	bool existentLocationDuplicate(const ServerConfig &server, const LocConfig &location);
	bool baseLocation(ServerConfig &server);
	void addRootToErrorUri(ServerConfig &server);
	std::string addPrefix(const std::string &uri, const std::string &prefix_);



	// Debug print methods
	void printServers(const std::vector<ServerConfig> &servers, std::ostream &os) const;
	void printServerConfig(const ServerConfig &server, std::ostream &os) const;
	void printLocationConfig(const LocConfig &loc, std::ostream &os) const;
	void printTree(const ConfigNode &node, const std::string &prefix, bool is_last,
	               std::ostream &os) const;
	static std::string joinArgs(const std::vector<std::string> &args);
};


#endif
