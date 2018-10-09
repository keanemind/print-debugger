all: main.cpp gdb_controller.h
	g++ -std=c++0x -g -o pdb main.cpp gdb_controller.cpp
