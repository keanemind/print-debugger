#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <exception>
#include <algorithm>
#include <csignal>
#include <sys/wait.h>
#include <cassert>
#include "gdb_controller.hpp"

using namespace GDB;


const char* NotRunningException::what() const throw() {
    return "GDB process is not running.";
}


bool Controller::is_initialized = false;
std::unordered_map<int, Controller*> Controller::running_gdbs = std::unordered_map<int, Controller*>();

void Controller::sigchld_handler(int sig_num, siginfo_t *sinfo, void *unused) {
    if (sinfo->si_code == CLD_EXITED || sinfo->si_code == CLD_KILLED) {
        // Terminated
        waitid(P_PID, sinfo->si_pid, nullptr, WEXITED | WNOHANG);
        // TODO: deal with this assert; clients may have their own child processes
        assert(running_gdbs.count(sinfo->si_pid));
        running_gdbs[sinfo->si_pid]->running = false;
        running_gdbs.erase(sinfo->si_pid);
    } else if (sinfo->si_code == CLD_STOPPED) {
        // Stopped
        waitid(P_PID, sinfo->si_pid, nullptr, WSTOPPED | WNOHANG);
        // TODO: deal with this assert; clients may have their own child processes
        assert(running_gdbs.count(sinfo->si_pid));
        assert(running_gdbs[sinfo->si_pid]->running == true);
        running_gdbs[sinfo->si_pid]->running = false;
    } else if (sinfo->si_code == CLD_CONTINUED) {
        // Continued
        waitid(P_PID, sinfo->si_pid, nullptr, WCONTINUED | WNOHANG);
        // TODO: deal with this assert; clients may have their own child processes
        assert(running_gdbs.count(sinfo->si_pid));
        assert(running_gdbs[sinfo->si_pid]->running == false);
        running_gdbs[sinfo->si_pid]->running = true;
    }
}

Controller::Controller() {
    if (!is_initialized) {
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sa.sa_sigaction = Controller::sigchld_handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            throw std::runtime_error(
                "Error setting up module: sigaction failed."
            );
        }
        is_initialized = true;
    }
}

Controller::~Controller() {
    if (running) {
        this->exit();
    }
}

bool Controller::is_running() {
    return running;
}

int Controller::get_pid() {
    return pid;
}

void Controller::spawn(std::string program_name) {
    if (this->running) {
        return;
    }

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
        ::exit(1);
    }
    // Parent
    // Close unused ends of pipes
    close(fd0[0]);
    close(fd1[1]);

    // Set up file
    this->in = fdopen(fd1[0], "r");

    // Wait for output and check that GDB started successfully
    char gdb_start_output[50];

    // Read up to five lines from GDB
    for (int i = 0; i < 5; i++) {
        // Give GDB 1 second to respond
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fd1[0], &read_set);
        timeval timeout = {.tv_sec = 3, .tv_usec = 0};
        int result = select(fd1[0] + 1, &read_set, NULL, NULL, &timeout);
        if (result == 0) { // GDB did not reply
            throw std::runtime_error(
                "No expected reply from GDB during startup."
            );
        }

        fgets(gdb_start_output, 50, this->in);

        if (!std::string(gdb_start_output).compare(0, 5, "(gdb)")) {
            std::cout << "GDB is running." << std::endl;
            this->running = true;
            running_gdbs[pid] = this;
            break;
        } else if (!strcmp(gdb_start_output, "EXECVP_ERROR")) {
            throw std::runtime_error("execvp() failure.");
        }
    }

    // If GDB never outputted anything expected (the for loop did not
    // get broken), log an error
    if (!running) {
        std::string err_msg = "Unexpected output from GDB during startup. "
                              "Last outputted line: ";
        err_msg += gdb_start_output;
        throw std::runtime_error(err_msg.c_str());
    }
}

std::string Controller::send(std::string command) {
    return send(command, "(gdb)");
}

std::string Controller::send(
    std::string command,
    std::string response_terminator
) {
    if (!running) {
        throw NotRunningException();
    }
    write(fd0[1], command.c_str(), command.size());

    // Give GDB 3 seconds to respond
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd1[0], &read_set);
    timeval timeout = {.tv_sec = 3, .tv_usec = 0};
    int result = select(fd1[0] + 1, &read_set, NULL, NULL, &timeout);
    if (result == 0) { // GDB did not reply
        throw std::runtime_error("No reply from GDB.");
    }

    std::string ret;
    char gdb_output[50];
    do {
        fgets(gdb_output, 50, this->in);
        ret.append(gdb_output);
    } while (
        std::string(gdb_output).compare(
            0, response_terminator.size(), response_terminator
        )
    );
    std::cout << ret << std::endl;
    return ret;
}

void Controller::exit() {
    std::string resp = send("-gdb-exit\r\n", "^exit");
    if (resp == "") {
        // TODO: try SIGTERM, then SIGKILL
        // GDB did not reply. Force kill.
        kill(pid, SIGKILL);
    }

    // Block until sigchld_handler is run (GDB finishes exiting).
    if (waitid(P_PID, pid, nullptr, WEXITED | WNOWAIT) == -1) {
        std::string err = "waitid() failed: ";
        err += strerror(errno);
        throw std::runtime_error(err.c_str());
    }
    close(fd0[1]);
    close(fd1[0]);
}

void Controller::run() {
    send("-exec-run\r\n");
}

void Controller::cont() {
    send("-exec-continue\r\n");
}

Breakpoint Controller::add_breakpoint(
    std::string filename,
    unsigned int line_no
) {
    send(
        "-break-insert " + filename + ":" + std::to_string(line_no) + "\r\n"
    );

    // TODO: Parse output, initialize Breakpoint object, return it.
    return Breakpoint(*this, 1, "", 1);
}

bool Controller::remove_breakpoint(int bp_no) {
    send("-break-delete " + std::to_string(bp_no) + "\r\n");

    //TODO: Parse output, return true or false.
    return true;
}


Breakpoint::Breakpoint(
    Controller& controller,
    int id,
    std::string filename,
    int line_no
) : controller(controller) {
    this->id = id;
    this->filename = filename;
    this->line_no = line_no;
    commands.push_back("continue");
}

int Breakpoint::get_id() {
    return id;
}

std::string Breakpoint::get_filename() {
    return filename;
}

int Breakpoint::get_line_no() {
    return line_no;
}

void Breakpoint::update_commands() {
    std::string combined;
    for (int i = 0; i < commands.size(); i++) {
        combined += "\"" + commands[i] + "\" ";
    }
    controller.send(
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

Breakpoint& Breakpoint::operator=(const Breakpoint& right) {
    this->controller = right.controller;
    this->id = right.id;
    this->filename = right.filename;
    this->line_no = right.line_no;
}
