/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Env.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/03 11:30:01 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/03 14:35:30 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ENV_HPP
#define ENV_HPP

# include "../../includes/webserv.hpp"
# include "../../includes/types.hpp"
# include "../Logger/Logger.hpp"
# include "cgi.hpp"

class Env {
private:
    std::map<std::string, std::string> vars;

public:
    Env() {};
	Env(ClientRequest &request) {
		set("REDIRECT_STATUS", "200");
		set("DOCUMENT_ROOT", std::getenv("PWD"));
		set("PATH_TRANSLATED", std::string(std::getenv("PWD")) + "/" + request.path);
		set("SCRIPT_FILENAME", std::string(std::getenv("PWD")) + "/" + request.path);
		set("SCRIPT_NAME", "/" + request.path);
		set("REQUEST_URI", "/" + request.uri);
		set("REQUEST_METHOD", CGIUtils::select_method(request.method));
		set("QUERY_STRING", request.query);
		if (request.method == POST) {
			set("CONTENT_TYPE", request.headers["content-type"]);
			set("CONTENT_LENGTH", request.headers["content-length"]);
		}

		set("GATEWAY_INTERFACE", "CGI/1.1");
		set("SERVER_PROTOCOL", request.version); // e.g. "HTTP/1.1"
		set("SERVER_NAME", request.headers["host"]);
		set("SERVER_PORT", "80");
		set("REMOTE_ADDR", "127.0.0.1");
	};

    // Set or update an environment variable
    void set(const std::string &key, const std::string &value) {
        vars[key] = value;
    };

    // Get an environment variable's value or empty string if not found
    std::string get(const std::string &key) const {
        std::map<std::string, std::string>::const_iterator it = vars.find(key);
        if (it != vars.end()) {
            return (it->second);
        }
        return ("");
    };

    // Remove a variable if it exists
    void unset(const std::string &key) {
        vars.erase(key);
    };

	char **to_envp() const {
    char **envp = new char*[vars.size() + 1];
    size_t i = 0;

    for (std::map<std::string, std::string>::const_iterator it = vars.begin(); it != vars.end(); ++it, ++i) {
        std::string var = it->first + "=" + it->second;

        // Allocate with new[], copy characters manually
        char* c_var = new char[var.size() + 1];
        for (size_t j = 0; j < var.size(); ++j) {
            c_var[j] = var[j];
        }
        c_var[var.size()] = '\0';

        envp[i] = c_var;
    }
    envp[vars.size()] = NULL;

    return envp;
}

static void free_envp(char **envp) {
    if (!envp) return;
    for (size_t i = 0; envp[i] != NULL; ++i) {
        delete [] envp[i];
    }
    delete [] envp;
}

/* 	// Convert to a null-terminated char** suitable for execve
    char **to_envp() const {
		char** envp = new char*[vars.size() + 1];
		size_t i = 0;

		for (std::map<std::string, std::string>::const_iterator it = vars.begin(); it != vars.end(); ++it, ++i) {
			std::string var = it->first + "=" + it->second;
			char* c_var = new char[var.size() + 1];
			for (size_t j = 0; j < var.size(); ++j)
				c_var[j] = var[j];
			c_var[var.size()] = '\0';
			envp[i] = c_var;
		}
		envp[vars.size()] = NULL;

		return (envp);
	};

    // Static helper to free envp arrays created by toEnvp()
    static void free_envp(char **envp) {
        if (!envp) return;
        for (size_t i = 0; envp[i] != NULL; ++i)
            delete[] envp[i];
        delete[] envp;
    }; */
};

#endif