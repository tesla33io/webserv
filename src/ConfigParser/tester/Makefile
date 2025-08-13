# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/06/19 11:43:52 by jalombar          #+#    #+#              #
#    Updated: 2025/06/23 19:45:04 by htharrau         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

#Compiler to be used
CXX				:= c++

#Compiler flags
CXXFLAGS		:= -Wall -Werror -Wextra -std=c++98 -pedantic

#Libraries to be linked(if any)
LDLIBS			:=

#Include directories
INCLUDES		:= -I./ -I./src

#Target executable
TARGET			:= tester

#Source files directory
SRC_DIR			:= ./

#Source files
SRC_FILES		+= ../ConfigParser.cpp
SRC_FILES		+= ../ServerStructure.cpp
SRC_FILES		+= ../ConfigHelper.cpp
SRC_FILES		+= ../ValidDirective.cpp
SRC_FILES		+= tester.cpp

#Object files directory
OBJ_DIR			:= obj/

#Object files
OBJ_FILES		:= $(patsubst %.cpp, $(OBJ_DIR)%.o, $(SRC_FILES))

#Dependency files directory
DEP_DIR			:= dep/

#Dependency files
DEPENDS			:= $(patsubst %.cpp, $(DEP_DIR)%.d, $(SRC_FILES))
-include $(DEPENDS)


############################
###### SHELL COMMANDS ######
############################

RM				:= /bin/rm -f
MKDIR			:= /bin/mkdir -p
TOUCH			:= /bin/touch


#############################
###### LOCAL LIBRARIES ######
#############################


############################
###### DEBUG SETTINGS ######
############################

ifeq ($(DEBUG), 1)
	CXXFLAGS	+= -fsanitize=address,undefined -D_GLIBCXX_DEBUG
	#CXXFLAGS	+= -g3 -O0
endif


################################
###### TARGET COMPILATION ######
################################

.DEFAULT_GOAL	:= all

all: ## Build this project
	$(MAKE) -j $(TARGET)

#Compilation rule for object files
$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@$(MKDIR) $(dir $@)
	@$(MKDIR) $(dir $(patsubst $(OBJ_DIR)%.o, $(DEP_DIR)%.d, $@))
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(patsubst $(OBJ_DIR)%.o, $(DEP_DIR)%.d, $@) $(INCLUDES) -c $< -o $@

#Rule for linking the target executable
$(TARGET): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ_FILES) $(INCLUDES) $(LDLIBS)
	-@echo -en "🚀 $(MAGENTA)" && ls -lah $(TARGET) && echo -en "$(RESET)"

##############################
###### ADDITIONAL RULES ######
##############################

clean: ## Clean objects and dependencies
	$(RM) $(OBJ_FILES)
	$(RM) -r $(OBJ_DIR)
	$(RM) $(DEPENDS)
	$(RM) -r $(DEP_DIR)

fclean: clean ## Restore project to initial state
	$(RM) $(TARGET)

re: fclean all ## Rebuild project

todo: ## Print todo's from source files
	find . -type f \( -name "*.cpp" -o -name "*.hpp" \) -print | grep -v ".venv" | xargs grep --color -Hn "// *TODO"

help: ## Show help info
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "$(CYAN)%-30s$(RESET) %s\n", $$1, $$2}'

.PHONY: all re clean fclean help

####################
###### COLORS ######
####################
#Color codes
RESET		:= \033[0m
BOLD		:= \033[1m
UNDERLINE	:= \033[4m

#Regular colors
BLACK		:= \033[30m
RED			:= \033[31m
GREEN		:= \033[32m
YELLOW		:= \033[33m
BLUE		:= \033[34m
MAGENTA		:= \033[35m
CYAN		:= \033[36m
WHITE		:= \033[37m

#Background colors
BG_BLACK	:= \033[40m
BG_RED		:= \033[41m
BG_GREEN	:= \033[42m
BG_YELLOW	:= \033[43m
BG_BLUE		:= \033[44m
BG_MAGENTA	:= \033[45m
BG_CYAN		:= \033[46m
BG_WHITE	:= \033[47m
