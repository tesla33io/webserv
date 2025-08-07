/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 14:47:40 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:14:30 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>  // for perror
#include <cstdlib> // for exit
#include <cstring> // for strncmp
#include <ctime>
#include <dirent.h> // for directory listing
#include <exception>
#include <fcntl.h>
#include <fstream> // for ifstream
#include <functional>
#include <iostream>
#include <map> // for map
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <stdint.h> // for uint16_t
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h> // for send
#include <sys/stat.h>
#include <sys/types.h> // for pid_t
#include <sys/wait.h>  // for waitpid
#include <unistd.h>    // for pipe, dup2, fork, exec
#include <utility>     // for makepair
#include <vector>      // for vector

#define CHUNK_SIZE 500

#endif
