/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ArgumentParser.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:02:42 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:18:41 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ARGUMENTPARSER_HPP
#define ARGUMENTPARSER_HPP

#include "includes/Webserv.hpp"

struct ServerArgs {
	std::string config_file;
	std::string prefix_path;
	bool show_help;
	bool show_version;
	int log_level; // 0=error, 1=warn, 2=info, 3=debug

	ServerArgs()
	    : config_file(""),
	      prefix_path(""),
	      show_help(false),
	      show_version(false),
	      log_level(1) {}
};

class ArgumentParser {
  private:
	std::vector<std::string> default_config_paths;
	std::vector<std::string> known_flags;

  public:
	ArgumentParser() {
		default_config_paths.push_back("webserver.conf");
		default_config_paths.push_back("./webserver.conf");
		default_config_paths.push_back("./config/webserver.conf");

		known_flags.push_back("--prefix-path");
		known_flags.push_back("--log-level");
	}

	ServerArgs parseArgs(int argc, char *argv[]) {
		ServerArgs args;
		std::vector<std::string> positional_args;

		for (int i = 1; i < argc; ++i) {
			std::string arg = argv[i];

			if (arg == "--help" || arg == "-h") {
				args.show_help = true;

			} else if (arg == "--version" || arg == "-v") {
				args.show_version = true;

			} else if (arg == "--prefix-path") {
				if (i + 1 < argc) {
					args.prefix_path = argv[++i];
				} else {
					throw std::runtime_error("--prefix-path requires a value");
				}

			} else if (arg.find("--prefix-path=") == 0) {
				args.prefix_path = arg.substr(14); // Skip "--prefix-path="

			} else if (arg == "--log-level") {
				if (i + 1 < argc) {
					args.log_level = parseLogLevel(argv[++i]);
				} else {
					throw std::runtime_error("--log-level requires a value");
				}

			} else if (arg.find("--log-level=") == 0) {
				args.log_level = parseLogLevel(arg.substr(12));

			} else if (arg.find("--") == 0) {
				throw std::runtime_error("Unknown option: " + arg);

			} else if (arg.find("-") == 0 && arg.length() > 1) {
				// Handle short options (could be combined like -dt)
				parseShortOptions(arg.substr(1), args);

			} else {
				// Positional argument (potential config file)
				positional_args.push_back(arg);
			}
		}

		if (!args.show_help && !args.show_version)
			args.config_file = determineConfigFile(positional_args);
		return args;
	}

	void printUsage(const std::string &program_name) {
		std::cout << "Usage: " << program_name << " [OPTIONS] [CONFIG_FILE]\n\n";
		std::cout << "Options:\n";
		std::cout << "  -h, --help              Show this help message\n";
		std::cout << "  -v, --version           Show version information\n";
		std::cout << "      --prefix-path PATH  Set prefix path for relative paths\n";
		std::cout << "      --log-level LEVEL   Set log level (error|warn|info|debug)\n";
		std::cout << "\nIf CONFIG_FILE is not specified, the following locations are tried:\n";

		for (std::vector<std::string>::const_iterator it = default_config_paths.begin();
		     it != default_config_paths.end(); ++it) {
			std::cout << "  " << *it << "\n";
		}
		std::cout << std::endl;
	}

  private:
	void parseShortOptions(const std::string &options, ServerArgs &args) {
		for (size_t j = 0; j < options.length(); ++j) {
			char opt = options[j];

			switch (opt) {
			case 'h':
				args.show_help = true;
				break;
			case 'v':
				args.show_version = true;
				break;
			default:
				std::string unknown_opt = "-";
				unknown_opt += opt;
			}
		}
	}

	int parseLogLevel(const std::string &level) {
		if (level == "error" || level == "0")
			return 0;
		if (level == "warn" || level == "1")
			return 1;
		if (level == "info" || level == "2")
			return 2;
		if (level == "debug" || level == "3")
			return 3;

		throw std::runtime_error("Invalid log level: " + level +
		                         " (use: error|warn|info|debug or 0-3)");
	}

	std::string determineConfigFile(const std::vector<std::string> &positional_args) {
		// If user provided a positional argument, assume it's the config file
		if (!positional_args.empty()) {
			std::string config_file = positional_args[0];

			if (positional_args.size() > 1) {
				std::cerr << "Warning: Extra arguments ignored: ";
				for (size_t i = 1; i < positional_args.size(); ++i) {
					std::cerr << positional_args[i];
					if (i < positional_args.size() - 1)
						std::cerr << ", ";
				}
				std::cerr << std::endl;
			}

			if (!fileExists(config_file)) {
				throw std::runtime_error("Config file not found: " + config_file);
			}

			return config_file;
		}

		// No config file specified, try defaults
		for (std::vector<std::string>::const_iterator it = default_config_paths.begin();
		     it != default_config_paths.end(); ++it) {
			if (fileExists(*it)) {
				std::cout << "Using default config: " << *it << std::endl;
				return *it;
			}
		}

		throw std::runtime_error("No config file found. Please specify a config file or "
		                         "create one in a default location.");
	}

	bool fileExists(const std::string &path) {
		struct stat buffer;
		return (stat(path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
	}
};

#endif
