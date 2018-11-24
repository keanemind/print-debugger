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
#include <chrono>
#include <thread>
#include "gdb_controller.hpp"

using namespace GDB;


const char* NotRunningException::what() const throw() {
    return "The GDB process is not running.";
}


const char* NoReplyException::what() const throw() {
    return "The GDB process did not reply.";
}


bool Controller::is_initialized = false;
std::unordered_map<int, Controller*> Controller::running_gdbs = std::unordered_map<int, Controller*>();

void Controller::sigchld_handler(int sig_num, siginfo_t *sinfo, void *unused) {
    if (sinfo->si_code == CLD_EXITED || sinfo->si_code == CLD_KILLED) {
        // Terminated
        waitid(P_PID, sinfo->si_pid, nullptr, WEXITED | WNOHANG);
        assert(running_gdbs.count(sinfo->si_pid));
        running_gdbs[sinfo->si_pid]->running = false;
        running_gdbs.erase(sinfo->si_pid);
    } else if (sinfo->si_code == CLD_STOPPED) {
        // Stopped
        waitid(P_PID, sinfo->si_pid, nullptr, WSTOPPED | WNOHANG);
        assert(running_gdbs.count(sinfo->si_pid));
        assert(running_gdbs[sinfo->si_pid]->running == true);
        running_gdbs[sinfo->si_pid]->running = false;
    } else if (sinfo->si_code == CLD_CONTINUED) {
        // Continued
        waitid(P_PID, sinfo->si_pid, nullptr, WCONTINUED | WNOHANG);
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
    return await_reply(response_terminator);
}

std::string Controller::await_reply(std::string response_terminator) {
    // Give GDB 3 seconds to respond
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd1[0], &read_set);
    timeval timeout = {.tv_sec = 3, .tv_usec = 0};
    int result = select(fd1[0] + 1, &read_set, NULL, NULL, &timeout);
    if (result == 0) { // GDB did not reply
        this->kill();
        throw NoReplyException();
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

void Controller::wait_for_stop(std::chrono::milliseconds timeout) {
    if (!running) {
        return;
    }
    auto start = std::chrono::high_resolution_clock::now();
    auto now = start;
    auto duration = std::chrono::seconds(1);
    auto margin = std::chrono::milliseconds(5);
    while (now < start + duration) {
        std::this_thread::sleep_for(
            start + duration - now + margin
        );
        if (!running) {
            break;
        }
        now = std::chrono::high_resolution_clock::now();
    }
}

void Controller::exit() {
    send("-gdb-exit\r\n", "^exit");
    close(fd0[1]);
    close(fd1[0]);

    // Block until sigchld_handler is run (GDB finishes exiting), with
    // a time limit of 1 second.
    wait_for_stop(std::chrono::milliseconds(1000));
    if (running) {
        // Make closed fds invalid so that kill can't accidentally close
        // a newly opened fd with the same number. This would only be a problem
        // with multithreading, which GDB Controller may someday use.
        fd0[1] = -1;
        fd1[0] = -1;
        this->kill();
    }
}

void Controller::kill() {
    if (running) {
        close(fd0[1]);
        close(fd1[0]);
        wait_for_stop(std::chrono::milliseconds(1000));
    }

    if (running) {
        ::kill(pid, SIGTERM);
        wait_for_stop(std::chrono::milliseconds(1000));
    }

    if (running) {
        ::kill(pid, SIGKILL);
    }
}

void Controller::run() {
    // Start running the target.
    send("-exec-run\r\n");

    // Get output from GDB running the target.
    await_reply("(gdb)");
}

void Controller::cont() {
    send("-exec-continue\r\n");
}

Breakpoint Controller::add_breakpoint(
    std::string filename,
    unsigned int line_no
) {
    std::string reply = send(
        "-break-insert " + filename + ":" + std::to_string(line_no) + "\r\n"
    );

    // Parse output to get Breakpoint object fields
    Breakpoint bp = Breakpoint(*this);
    unsigned int bracket_open = reply.find("{");
    unsigned int bracket_close = reply.rfind("}");
    unsigned int start = bracket_open + 1;
    unsigned int end = reply.find(',', start);
    while (end < bracket_close) {
        // Get field name
        int loc_equal = reply.find('=', start);
        int field_name_len = loc_equal - start;
        std::string field_name = reply.substr(start, field_name_len);

        // Get value as string (starts at loc_equal + 2)
        int value_len = end - 1 - (loc_equal + 2);
        std::string value = reply.substr(loc_equal + 2, value_len);

        if (field_name == "number") {
            bp.id = std::stoi(value);
        } else if (field_name == "file") {
            bp.filename = value;
        } else if (field_name == "line") {
            bp.line_no = std::stoi(value);
        }

        start = end + 1;
        end = reply.find(',', start);
    }

    // Even if the return is assigned somewhere, this will actually call the
    // constructor only once, so update_commands() is not going to
    // inefficiently be called twice. 
    return bp;
}

bool Controller::remove_breakpoint(int bp_no) {
    send("-break-delete " + std::to_string(bp_no) + "\r\n");
    return true;
}


Breakpoint::Breakpoint(Controller& controller) : controller(controller) {}

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
    for (unsigned i = 0; i < commands.size(); i++) {
        combined += "\"" + commands[i] + "\" ";
    }
    controller.send(
        "-break-commands " + std::to_string(id) + " " + combined + "\r\n"
    );
}

void Breakpoint::add_command(std::string command) {
    commands.push_back(command);
    update_commands();
}

bool Breakpoint::remove_command(std::string command) {
    auto it = std::find(commands.begin(), commands.end(), command);
    if (it != commands.end()) {
        commands.erase(it);
        update_commands();
        return true;
    }
    return false;
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
    return *this;
}
