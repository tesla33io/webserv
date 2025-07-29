
#include "config_parser.hpp"

// Remove comments and trim
std::string ConfigParser::preProcess(const std::string& line) const {
	size_t hashpos = line.find('#');
	std::string newline = (hashpos != std::string::npos) ? line.substr(0, hashpos) : line;
	return (su::trim(newline));
}

// isUtil
bool ConfigParser::isBlockStart(const std::string& line) const {
	return (su::ends_with(line, "{"));
}
bool ConfigParser::isBlockEnd(const std::string& line) const {
	return (line == "}") ;
}
bool ConfigParser::isDirective(const std::string& line) const {
	return (su::ends_with(line, ";"));
}

// Tokenize
std::vector<std::string> ConfigParser::tokenize(const std::string& line) const {
	std::istringstream iss(line);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token)
		tokens.push_back(token);
	return tokens;
}


// Valid IPv4
bool ConfigParser::isValidIPv4(const std::string& ip) {
	std::istringstream iss(ip);
	std::string token;
	int count = 0;
	while (std::getline(iss, token, '.')) { 
		if (++count > 4) 
			return false;
		if (token.empty()) 
			return false;
		// Check characters are digits 0-255
		for (size_t i = 0; i < token.length(); ++i) {
			if (!isdigit(token[i])) 
				return false;
		}
		int num = atoi(token.c_str());
		if (num < 0 || num > 255) 
			return false;
	}
	return (count == 4);
}


// Helper function to check if string contains quotes
bool ConfigParser::hasQuotes(const std::string& str) {
	return str.find('"') != std::string::npos;
}

// Helper function to check if string contains ".."
bool ConfigParser::hasDotDot(const std::string& str) {
	return str.find("..") != std::string::npos;
}

// Helper function to check if URL is HTTP/HTTPS
bool ConfigParser::isHttp(const std::string& url) {
	if (url.empty()) 
		return false;
	return su::starts_with(url, "http://") || su::starts_with(url, "https://");
}

// Helper function to check if file ends with .html
bool ConfigParser::isHtml(const std::string& path) {
	return su::ends_with(path, ".html");
}


// for multi-context directives (e.g. "server", "location")
std::vector<std::string> ConfigParser::makeVector(const std::string& a, const std::string& b) const{
	std::vector<std::string> v;
	v.push_back(a);
	v.push_back(b);
	return v;
}


/////////////
// PRINT TREE
std::string ConfigParser::joinArgs(const std::vector<std::string>& args) const {
	if (args.empty()) return "";
	
	std::string result = args[0];
	for (size_t i = 1; i < args.size(); ++i) {
		result += " " + args[i];
	}
	return result;
}
void ConfigParser::printTree(const ConfigNode& node, const std::string& prefix, 
							 bool isLast, std::ostream &os) const {
	os << prefix;
	os << (isLast ? "└── " : "├── ");
	os << node.name_; 
	
	if (!node.args_.empty()) { 
		os << " " << joinArgs(node.args_);
	}
	
	os << " " << node.line_ << std::endl; 
	
	std::string childPrefix = prefix + (isLast ? "    " : "│   ");
	
	for (size_t i = 0; i < node.children_.size(); ++i) {  
		bool childIsLast = (i == node.children_.size() - 1); 
		printTree(node.children_[i], childPrefix, childIsLast, os); 
	}
}

////////////////
// PRINT STRUCT
void ConfigParser::printServers(const std::vector<ServerConfig>& servers, std::ostream &os) const {
	os << "=== Configuration Summary ===\n";
	os << "Total servers: " << servers.size() << "\n\n";
	
	for (size_t i = 0; i < servers.size(); ++i) {
		os << "Server " << (i + 1) << ":\n";
		printServerConfig(servers[i], os);  
		os << "\n";
	}
}
void ConfigParser::printLocationConfig(const LocConfig &loc, std::ostream &os) const {
	os << "  Location: " << loc.path;
	os << "\n";
	
	if (!loc.root.empty())
		os << "    Root: " << loc.root << "\n";
	
	if (!loc.alias.empty())
		os << "    Alias: " << loc.alias << "\n";
	
	if (!loc.index.empty()) {
		os << "    Index: " << loc.index << "\n";
	}
	
	// ADD THIS: Print upload_path if it's set
	if (!loc.upload_path.empty()) {
		os << "    Upload path: " << loc.upload_path << "\n";
	}
	
	os << "    Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
	
	if (!loc.allowed_methods.empty()) {
		os << "    Allowed methods: ";
		for (size_t i = 0; i < loc.allowed_methods.size(); ++i)
			os << loc.allowed_methods[i] << (i + 1 < loc.allowed_methods.size() ? ", " : "");
		os << "\n";
	}
	
	if (loc.hasReturn()) {
		os << "    Return directive: " << loc.return_code;
		if (!loc.return_target.empty())
			os << " " << loc.return_target;
		os << "\n";
	}
		
	if (!loc.cgi_extensions.empty()) {
		os << "    CGI extensions:\n";
		for (std::map<std::string, std::string>::const_iterator it = loc.cgi_extensions.begin(); 
			 it != loc.cgi_extensions.end(); ++it) {
			os << "      " << it->first << " -> " << it->second << "\n";
		}
	}
}
void ConfigParser::printServerConfig(const ServerConfig &server, std::ostream &os) const {
	os << "Server on " << server.host << ":" << server.port << "\n";
	
	os << "  Client max body size: " << server.client_max_body_size << " bytes\n";
	
	if (!server.error_pages.empty()) {
		os << "  Error pages:\n";
		for (std::map<uint16_t, std::string>::const_iterator it = server.error_pages.begin(); 
			 it != server.error_pages.end(); ++it) {
			os << "    " << it->first << " -> " << it->second << "\n";
		}
	}
	
	if (!server.locations.empty()) {
		for (size_t i = 0; i < server.locations.size(); ++i) {
			printLocationConfig(server.locations[i], os); 
			if (i < server.locations.size() - 1) {
				os << "\n";
			}
		}
	}
	
	os << "-------------------------------------------\n";
}


