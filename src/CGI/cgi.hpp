/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 14:09:28 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/22 14:01:05 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
#define CGI_HPP

# include "../../includes/types.hpp"
# include "../../includes/webserv.hpp"
# include "../Logger/Logger.hpp"
# include "../HttpServer/HttpServer.hpp"
#include "../HttpServer/Response.hpp"

namespace CGIUtils {
std::string get_interpreter(std::string &path);
bool handle_CGI_request(ClientRequest &request, int fd);
} // namespace CGIUtils

#endif
