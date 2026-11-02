# Host-side test of the line state machine: make -C test
CXX ?= clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -O1 -g

line_test: line_test.cpp ../nazar_line/line.cpp ../nazar_line/line.h ../nazar_line/config.h
	$(CXX) $(CXXFLAGS) -o $@ line_test.cpp ../nazar_line/line.cpp

run: line_test
	./line_test

.PHONY: run
.DEFAULT_GOAL := run
