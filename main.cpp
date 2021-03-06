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
    if (argc == 1) {
        std::cout << "You must specify a program to debug as an argument."
        << std::endl;
        return 0;
    }
    std::cout << "Starting GDB." << std::endl;
    GDB::Controller gdb;
    gdb.spawn(std::string(argv[1]));
    GDB::Breakpoint bp = gdb.add_breakpoint(std::string("hello.cpp"), 6);
    bp.add_command("print cout");
    bp.add_command("print main");
    bp.add_command("print my_str");
    bp.add_command("continue");
    bp.remove_command("print cout");
    GDB::Breakpoint bp1 = gdb.add_breakpoint("hello.cpp", 5);
    bp1.add_command("continue");
    gdb.run();
    gdb.exit();
}
