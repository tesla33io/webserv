# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/06/21 09:17:15 by htharrau          #+#    #+#              #
#    Updated: 2025/06/21 09:19:56 by htharrau         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #


CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -MMD -MP


NAME     = webserv
SRCS     = main.cpp serverLoop.cpp serverCreation.cpp configParsing.cpp
OBJS     = $(SRCS:.cpp=.o)
DEPS     = $(OBJS:.o=.d)


all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re