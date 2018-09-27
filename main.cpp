#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc == 0) {
        // Print help
    }
    std::cout << "Running GDB." << std::endl;
    int fd0[2]; // PDB -> GDB
    int fd1[2]; // GDB -> PDB
    int pid = fork();
    if (pid == -1) {
        std::cout << "Fork failed." << std::endl;
    } else if (pid == 0) {
        // Child
        close(fd0[1]);
        // Close stdin and duplicate the input end of the pipe to stdin
        dup2(fd0[0], 0);
        int err = execvp("gdb", argv + 1);
        if (err == -1) {
            std::cout << "execvp failure:" << strerror(errno) << std::endl;
        }
        exit(1);
    }
    // Parent
    close(fd0[0]);
    // Close stdout and duplicate the output end of the pipe to stdout
    dup2(fd0[1], 1);
}
