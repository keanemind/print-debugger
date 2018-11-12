#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <sys/wait.h>
#include <string>
#include "gdb_controller.hpp"

int main(int argc, char** argv) {
    if (argc == 0) {
        // Print help
    }
    std::cout << "Starting GDB." << std::endl;
    GDBController gdb;
    gdb.spawn(std::string(argv[1]));
    gdb.add_breakpoint(std::string("hello.cpp"), 4);
    gdb.set_breakpoint_print(1, "main");
    gdb.run();
    gdb.kill();
}
