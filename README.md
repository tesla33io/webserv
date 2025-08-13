## Simple HTTP Web Server in C++98

Lightweight HTTP web server implemented in C++98, designed to provide a straightforward yet robust solution for serving web content and handling basic HTTP requests.

---

**Key Features**

- **C++98 Compatibility:** Written entirely in C++98
- **HTTP/1.1 Support:** Handles core HTTP/1.1 methods, including GET, POST, and DELETE
- **Static File Serving:** Serves static files from a configurable root directory
- **Configurable Server:** Uses a configuration file (nginx-style) to define server behavior, ports, document roots, error pages, and more
- **Concurrent Connections:** Supports multiple simultaneous client connections using non-blocking I/O
- [TBD] **Custom Error Pages:** Allows definition of custom error pages
- [TBD] **CGI Support:** Can execute CGI scripts for dynamic content generation
- **Simple Build & Run:** Minimal dependencies; just a C++98-compatible compiler and a Linux OS (for epoll)

---

**Getting Started**

**Prerequisites**
- C++98-compatible compiler (e.g., g++)
- Linux OS (tested on Arch and Ubuntu)

**Building the Project**
```sh
make
```

**Running the Server**
```sh
./webserv [configuration_file]
```

**Accessing the Server**
- Open your browser and navigate to `http://localhost:PORT/`
- Or use `curl` for command-line testing

---

**Configuration**

The server uses a configuration file to define its behavior. Example configuration options include:

- Listening ports
- Server names
- Root directory for static files
- Error pages
- CGI script paths

Refer to the example config file in the repository for details

---

**Project Structure**

- **Server Core:** Manages sockets, connections, and the main event loop
- **Request Parser:** Interprets incoming HTTP requests
- **Response Builder:** Constructs and sends HTTP responses
- **Configuration Loader:** Parses and applies server settings
- **CGI Handler:** Executes CGI scripts for dynamic content

---

**License**

This project is open source. See the LICENSE file for details.

---

**Acknowledgements**

Inspired by classic web server projects and the 42 curriculum.
# test_webserv
