#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <exception>
#include "gdb_controller.h"

void GDBController::spawn(std::string program_name) {
    if (this->running) {
        return;
    }

    int fd0[2]; // PDB -> GDB
    int fd1[2]; // GDB -> PDB
    pipe(fd0);
    pipe(fd1);
    int pid = fork();
    if (pid == -1) {
        throw std::runtime_error("Fork failed.");
    } else if (pid == 0) {
        // Child
        // Close unused ends of pipes
        close(fd0[1]);
        close(fd1[0]);

        // Set up argument list
        char const* gdb_argv[6] = {
            "gdb",
            "--interpreter=mi",
            "-n",
            "-q",
            program_name.c_str(),
            nullptr
        };

        // Duplicate ends of the pipes to stdin and stdout
        dup2(fd0[0], 0);
        dup2(fd1[1], 1);

        // Run GDB
        execvp("gdb", const_cast<char**>(gdb_argv));

        // execvp error
        write(fd1[1], "EXECVP_ERROR", 13);
        exit(1);
    }
    // Parent
    this->pid = pid;

    // Close unused ends of pipes
    close(fd0[0]);
    close(fd1[1]);

    // Set up files
    this->out = fdopen(fd0[1], "w");
    this->in = fdopen(fd1[0], "r");

    // Wait for output and check that GDB started successfully
    char gdb_start_output[50];

    // Read up to five lines from GDB
    int i;
    for (i = 0; i < 5; i++) {
        fgets(gdb_start_output, 50, this->in);

        if (!std::string(gdb_start_output).compare(0, 5, "(gdb)")) {
            std::cout << "GDB is running." << std::endl;
            this->running = true;
            break;
        } else if (!strcmp(gdb_start_output, "EXECVP_ERROR")) {
            std::cout << "EXECVP FAILED." << std::endl;
            break;
        }
    }

    // If GDB never outputted anything expected (the for loop did not
    // get broken), log an error
    if (i >= 5) {
        std::cout << "Unexpected output from GDB during startup. ";
        std::cout << "Last outputted line: " << std::endl;
        std::cout << gdb_start_output << std::endl;
    }
}