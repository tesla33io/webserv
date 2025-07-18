/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 14:09:28 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/18 17:33:26 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
#define CGI_HPP

#include "../../includes/types.hpp"
#include "../../includes/webserv.hpp"
#include "../Logger/Logger.hpp"

namespace CGIUtils {
std::string select_method(RequestMethod &method);
std::string get_interpreter(std::string &path);
bool handle_CGI_request(ClientRequest &request, int fd);
} // namespace CGIUtils

#endif
