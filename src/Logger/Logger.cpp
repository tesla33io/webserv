#include "Logger.hpp"

int main(void) {
  // Create logger with file output
  Logger logger("webserver.log", Logger::DEBUG, true);

  // Test different log levels
  logger.debug("Server initializing...");
  logger.info("Server started on port 8080");
  logger.warning("High memory usage detected");
  logger.error("Failed to connect to database");
  logger.critical("Server shutting down due to critical error");

  // Test with prefixes for different modules
  logger.logWithPrefix(Logger::INFO, "HTTP",
                       "New connection from 192.168.1.100");
  logger.logWithPrefix(Logger::ERROR, "DB", "Connection timeout");

  // Change log level and test
  logger.setLogLevel(Logger::WARNING);
  logger.debug("This won't be logged");
  logger.info("This won't be logged either");
  logger.warning("This will be logged");
}

