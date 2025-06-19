/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 14:45:55 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/19 15:07:39 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

# include "../../webserv.hpp"

namespace GeneralUtils {
	std::string to_upper(std::string &s);
	std::string to_lower(std::string &s);
}

#endif