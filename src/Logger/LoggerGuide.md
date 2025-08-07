## Quick Start

Just drop the header file and you're good to go:

Constructor arguments (filename is optional if the Console output is enabled)
```cpp
Logger logger(filename, minLogLevel, enableConsole);
```

---

```cpp
#include "Logger.hpp"

int main() {
    // Create a logger that writes to both console and file
    Logger logger("my_server.log");
    
    logger.info("Server starting up...");
    logger.info("Listening on port 8080");
    
    return 0;
}
```

That's it! Your logs are now going to both the console and `my_server.log`.

## The Basics

### Log Levels (From Chattiest to Most Important)

- **DEBUG**: The nitty-gritty details. Usually turned off in production.
- **INFO**: General information about what's happening.
- **WARNING**: Something's not quite right, but we're handling it.
- **ERROR**: Something broke, but the server's still running.
- **CRITICAL**: Everything's on fire. Time to panic.

### Simple Logging

```cpp
Logger logger("webserver.log");

logger.debug("Processing request headers");
logger.info("New connection: 127.0.0.1");
logger.warn("Connection closed by client");
logger.error("Failed to send response back to client");
logger.critical("Out of memory! Server shutting down");
```

## Getting Fancy

### Filter Out the Noise

Don't want to see debug messages cluttering up your production logs?

```cpp
Logger logger("prod.log", Logger::INFO);  // Only INFO and above
logger.debug("You won't see this");
logger.info("But you'll see this");
```

### Console vs File Output

```cpp
Logger logger("server.log");

// Turn off console output (logs only go to file)
logger.setConsoleOutput(false);

// Turn off file output (logs only go to console)
logger.setFileOutput(false);
```

### Module Prefixes (The Good Stuff)

This is where it gets useful for web servers. Tag your logs so you know which part of your server is talking:

```cpp
Logger logger("webserver.log");

logger.logWithPrefix(Logger::INFO, "HTTP", "GET /api/users - 200 OK");
logger.logWithPrefix(Logger::ERROR, "HTTP", "POST /api/login - 401 Unauthorized");

logger.logWithPrefix(Logger::INFO, "SOC", "New client connected");
logger.logWithPrefix(Logger::ERROR, "SOC", "Connection terminated");
```

Your logs will look like this:
```
[2024-06-17 14:30:25] [INFO] [HTTP] GET /api/users - 200 OK
[2024-06-17 14:30:26] [ERROR] [SOC] Connection terminated
```

## Web Server Example

Here's how you might structure logging in an actual web server:

```cpp
class WebServer {
private:
    Logger logger;
    
public:
    WebServer() : logger("webserver.log", Logger::INFO) {
        logger.info("WebServer initializing...");
    }
    
    void start() {
        logger.info("Server started on port 8080");
        logger.logWithPrefix(Logger::INFO, "CONFIG", "Max connections: 1000");
        logger.logWithPrefix(Logger::INFO, "CONFIG", "Timeout: 30s");
    }
    
    void handleRequest(const std::string& method, const std::string& path) {
        std::stringstream ss;
        ss << method << " " << path << " - Processing";
        logger.logWithPrefix(Logger::DEBUG, "HTTP", ss.str());
        
        // ... handle request ...
        
        logger.logWithPrefix(Logger::INFO, "HTTP", method + " " + path + " - 200 OK");
    }
    
    void onDatabaseError(const std::string& error) {
        logger.logWithPrefix(Logger::ERROR, "DB", error);
    }
};
```

## Tips

### Check Before You Log (Performance)

If you're doing expensive string operations for debug logs:

```cpp
if (logger.isLevelEnabled(Logger::DEBUG)) {
    std::string expensiveDebugInfo = buildComplexDebugString();
    logger.debug(expensiveDebugInfo);
}
```

### Different Loggers for Different Things

```cpp
Logger accessLogger("access.log", Logger::INFO);
Logger errorLogger("errors.log", Logger::ERROR);
Logger debugLogger("debug.log", Logger::DEBUG);

// Use them for different purposes
accessLogger.info("GET /index.html - 200");
errorLogger.error("Socket connection failed");
debugLogger.debug("Request headers parsed successfully");
```

## What You Get in the Log Files

Each log entry looks like this:
```
[2024-06-17 14:30:25] [INFO] [HTTP] Server started on port 8080
[2024-06-17 14:30:26] [ERROR] [SOC] Socket connection failed
[2024-06-17 14:30:27] [DEBUG] Request processing complete
```

