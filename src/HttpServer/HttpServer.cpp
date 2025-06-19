#include "HttpServer.hpp"

int main(int argc, char *argv[]) {
    int port = 8080;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number. Using default port 8080.\n";
            port = 8080;
        }
    }

    WebServer server(port);

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server\n";
        return 1;
    }

    server.run();

    return 0;
}
