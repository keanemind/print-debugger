all: main.cpp gdb_controller.hpp gdb_controller.cpp
	g++ -std=c++0x -g -o pdb main.cpp gdb_controller.cpp
