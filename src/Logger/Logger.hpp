/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 13:59:18 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:17:35 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "includes/Webserv.hpp"

class Logger {
  public:
	enum LogLevel { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, CRITICAL = 4 };

	Logger(const std::string &filename = "", LogLevel minLogLevel = INFO, bool enableConsole = true)
	    : minLevel(minLogLevel),
	      consoleOutput(enableConsole),
	      fileOutput(false),
	      logFileName(filename) {

		if (!filename.empty()) {
			logFile.open(filename.c_str(), std::ios::app);
			if (logFile.is_open()) {
				fileOutput = true;
			} else {
				std::cerr << "Warning: Could not open log file: " << filename << std::endl;
			}
		}
	}

	~Logger() {
		if (logFile.is_open()) {
			logFile.close();
		}
	}

	// Set minimum log level
	void setLogLevel(LogLevel level) { minLevel = level; }

	// Enable/disable console output
	void setConsoleOutput(bool enable) { consoleOutput = enable; }

	// Enable/disable file output
	void setFileOutput(bool enable) { fileOutput = enable && logFile.is_open(); }

	// Main logging function
	void log(LogLevel level, const std::string &message) {
		if (level < minLevel) {
			return;
		}

		std::string formattedMessage = formatMessage(level, message);

		// Output to console
		if (consoleOutput) {
			if (level >= ERROR) {
				std::cerr << formattedMessage << std::endl;
			} else {
				std::cout << formattedMessage << std::endl;
			}
		}

		// Output to file
		if (fileOutput && logFile.is_open()) {
			logFile << formattedMessage << std::endl;
			logFile.flush(); // Ensure immediate write
		}
	}

	// Convenience methods for different log levels
	void debug(const std::string &message) { log(DEBUG, message); }

	void info(const std::string &message) { log(INFO, message); }

	void warn(const std::string &message) { log(WARNING, message); }

	void error(const std::string &message) { log(ERROR, message); }

	void critical(const std::string &message) { log(CRITICAL, message); }

	// Log with custom prefix (useful for different modules)
	void logWithPrefix(LogLevel level, const std::string &prefix, const std::string &message) {
		std::stringstream ss;
		ss << "[" << prefix << "] " << message;
		log(level, ss.str());
	}

	// Get current log level
	LogLevel getLogLevel() const { return minLevel; }

	// Check if logging is enabled for a specific level
	bool isLevelEnabled(LogLevel level) const { return level >= minLevel; }

  private:
	std::ofstream logFile;
	LogLevel minLevel;
	bool consoleOutput;
	bool fileOutput;
	std::string logFileName;

	// Get current timestamp as string
	std::string getCurrentTime() const {
		time_t rawtime;
		struct tm *timeinfo;
		char buffer[80];

		time(&rawtime);
		timeinfo = localtime(&rawtime);

		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
		return std::string(buffer);
	}

	// Convert log level to string
	std::string levelToString(LogLevel level) const {
		switch (level) {
		case DEBUG:
			return "DEBUG";
		case INFO:
			return "INFO";
		case WARNING:
			return "WARNING";
		case ERROR:
			return "ERROR";
		case CRITICAL:
			return "CRITICAL";
		default:
			return "UNKNOWN";
		}
	}

	// Format the log message
	std::string formatMessage(LogLevel level, const std::string &message) const {
		std::stringstream ss;
		ss << "[" << getCurrentTime() << "] "
		   << "[" << levelToString(level) << "] " << message;
		return ss.str();
	}
};

#endif // LOGGER_HPP
