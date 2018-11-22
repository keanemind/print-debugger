#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <exception>
#include <algorithm>
#include "gdb_controller.hpp"

using namespace GDB;

Interface::Interface() {
    // TODO: deal with this absurdity
}

Interface::Interface(int fd0[2], int fd1[2]) {
    this->fd0[0] = fd0[0];
    this->fd0[1] = fd0[1];
    this->fd1[0] = fd1[0];
    this->fd1[1] = fd1[1];
    this->in = fdopen(fd1[0], "r");
}

bool Interface::check_startup_output() {
    char gdb_start_output[50];

    // Read up to five lines from GDB
    int i;
    for (i = 0; i < 5; i++) {
        fgets(gdb_start_output, 50, this->in);

        if (!std::string(gdb_start_output).compare(0, 5, "(gdb)")) {
            std::cout << "GDB is running." << std::endl;
            return true;
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
    return false;
}

std::string Interface::send(std::string command) {
    return send(command, "(gdb)");
}

std::string Interface::send(
    std::string command,
    std::string response_terminator
) {
    write(fd0[1], command.c_str(), command.size());
    char gdb_output[50];
    do {
        fgets(gdb_output, 50, this->in);
        std::cout << gdb_output;
    } while (
        std::string(gdb_output).compare(
            0, response_terminator.size(), response_terminator
        )
    );
    std::cout << std::endl;

    // TODO: return actual response
    return std::string();
}

void Controller::spawn(std::string program_name) {
    if (this->running) {
        return;
    }

    int fd0[2];
    int fd1[2];

    pipe(fd0);
    pipe(fd1);
    pid = fork();
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
    // Close unused ends of pipes
    close(fd0[0]);
    close(fd1[1]);

    // Create Interface
    interface = Interface(fd0, fd1);

    // Wait for output and check that GDB started successfully
    if (!interface.check_startup_output()) {
        // Throw exception
    }
    this->running = true;
}

void Controller::kill() {
    if (!running) {
        return;
    }
    interface.send("-gdb-exit\r\n", "^exit");
}

void Controller::run() {
    interface.send("-exec-run\r\n");
}

void Controller::cont() {
    interface.send("-exec-continue\r\n");
}

Breakpoint& Controller::add_breakpoint(
    std::string filename,
    unsigned int line_no
) {
    interface.send(
        "-break-insert " + filename + ":" + std::to_string(line_no) + "\r\n"
    );

    // TODO: Parse output, initialize Breakpoint object, return it.
    Breakpoint bp = Breakpoint(*this, interface, 1, "", 1);
    breakpoints.insert({1, bp});
    return breakpoints.at(1);
}

Breakpoint::Breakpoint(
    Controller& controller,
    Interface& interface,
    int id,
    std::string filename,
    int line_no
) : controller(controller), interface(interface) {
    this->id = id;
    this->filename = filename;
    this->line_no = line_no;
    commands.push_back("continue");
}

Breakpoint& Breakpoint::operator=(const Breakpoint& right) {
    this->controller = right.controller;
    this->interface = right.interface;
    this->id = right.id;
    this->filename = right.filename;
    this->line_no = right.line_no;
}

void Breakpoint::update_commands() {
    std::string combined;
    for (int i = 0; i < commands.size(); i++) {
        combined += "\"" + commands[i] + "\" ";
    }
    interface.send(
        "-break-commands " + std::to_string(id) + " " + combined + "\r\n"
    );
}

void Breakpoint::add_command(std::string command) {
    commands.pop_back();
    commands.push_back(command);
    commands.push_back("continue");
    update_commands();
}

bool Breakpoint::remove_command(std::string command) {
    auto it = std::find(commands.begin(), commands.end(), command);
    if (it != commands.end()) {
        commands.pop_back();

        std::swap(*it, commands.back());
        commands.pop_back();

        commands.push_back("continue");
        update_commands();
    }
}

void Breakpoint::clear_commands() {
    commands.clear();
    update_commands();
}
