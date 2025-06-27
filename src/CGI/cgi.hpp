/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 14:09:28 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/27 16:13:14 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef CGI_HPP
#define CGI_HPP

# include "../../includes/webserv.hpp"
# include "../../includes/types.hpp"
#include "../Logger/Logger.hpp"

class Env {
private:
    std::map<std::string, std::string> vars;

public:
    Env() {};
	Env(ClientRequest &request) {
		set("REQUEST_METHOD", select_method(request.method));
		set("QUERY_STRING", request.query);
		set("CONTENT_TYPE", request.headers["content-type"]);
		set("CONTENT_LENGTH", std::to_string(request.body.length()));
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

	// Convert to a null-terminated char** suitable for execve
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
            free(envp[i]);
        delete[] envp;
    };
};

#endif