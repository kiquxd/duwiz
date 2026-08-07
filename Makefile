.PHONY: run-linux run-mac

LINK_FLAGS = -lftxui-component -lftxui-dom -lftxui-screen
INCLUDES = -I./src
CXX_FLAGS = -std=c++20 -o build/app
OS = $(shell uname -s)
SRCS = $(shell find src -name "*.cpp")

ifneq ($(OS), Linux)
	INCLUDES += -I$$(brew --prefix)/include
	LINK_FLAGS += -L$$(brew --prefix)/lib
endif

run:
	mkdir -p build
	g++ $(SRCS) $(CXX_FLAGS) $(INCLUDES) $(LINK_FLAGS)
