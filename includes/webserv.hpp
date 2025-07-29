/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 14:47:40 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/29 16:10:35 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
#define WEBSERV_HPP

# include <iostream>
# include <fstream>      // for ifstream
# include <sstream>
# include <string>
# include <map>          // for map
# include <vector>       // for vector
# include <cstdlib>
# include <unistd.h>     // for pipe, dup2, fork, exec
# include <sys/types.h>  // for pid_t
# include <sys/wait.h>   // for waitpid
# include <cstdlib>      // for exit
# include <cstdio>       // for perror
# include <cstring>      // for strncmp
# include <sys/socket.h> // for send
//# include <stdint.h>     // for uint16_t

# define CHUNK_SIZE 500

#endif
