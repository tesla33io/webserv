# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/07/01 08:40:33 by jalombar          #+#    #+#              #
#    Updated: 2025/07/04 10:02:25 by jalombar         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

CC = c++   
FLAGS = -Wall -Werror -Wextra
STD98 = -std=c++98
SRCS = tester.cpp ../cgi.cpp
HEADER = ../cgi.hpp
NAME = tester
OUT_PATH = ../../../$(NAME)

all: $(OUT_PATH)

$(OUT_PATH): $(SRCS)
	@$(CC) $(FLAGS) $(STD98) -o $(OUT_PATH) $(SRCS)
	
clean:

fclean: clean
	rm -f $(OUT_PATH)

re: fclean all