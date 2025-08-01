/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 14:47:40 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/01 10:55:19 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <cstdio> // for perror
#include <cstdlib>
#include <cstdlib> // for exit
#include <cstring> // for strncmp
#include <fstream> // for ifstream
#include <iostream>
#include <map> // for map
#include <sstream>
#include <string>
#include <sys/socket.h> // for send
#include <sys/types.h>  // for pid_t
#include <sys/wait.h>   // for waitpid
#include <unistd.h>     // for pipe, dup2, fork, exec
#include <utility>      // for makepair
#include <vector>       // for vector
//# include <stdint.h>     // for uint16_t

#define CHUNK_SIZE 500

#endif
