#include "config_parser.hpp"
#include "../Logger/Logger.hpp"


int main(int argc, char **argv) {

	if (argc != 2) {
		std::cerr << "webserv: no configuration file specified" << std::endl;
		std::cerr << "Usage: " << argv[0] << " <configuration_file>" << std::endl;
		return 1;
	}

	ConfigParser configparser;
	std::vector<ServerConfig> servers;

	if (!configparser.loadConfig(argv[1], servers)) {
		std::cerr << "Error: Failed to open or parse configuration file '" << argv[1] << "'" << std::endl;
		std::cerr << "Please check the configuration file syntax and try again." << std::endl;
		return 1;
	}

	return (0);
}
