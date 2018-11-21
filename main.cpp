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
    GDB::Controller gdb;
    gdb.spawn(std::string(argv[1]));
    GDB::Breakpoint bp = gdb.add_breakpoint(std::string("hello.cpp"), 6);
    bp.add_command("print main");
    bp.add_command("print my_str");
    gdb.run();
    gdb.kill();
}
