#ifndef GDB_CONTROLLER_HPP
#define GDB_CONTROLLER_HPP 1
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <exception>


namespace GDB {
    class Breakpoint;
    class Controller;

    struct NotRunningException : public std::exception {
        const char* what() const throw();
    };

    /* A GDB breakpoint. */
    class Breakpoint {
        Controller& controller;

        int id; // breakpoint number
        std::string filename;
        int line_no;
        std::vector<std::string> commands;

        /* Update GDB with the current command list. */
        void update_commands();

    public:
        Breakpoint(
            Controller& controller,
            int id,
            std::string filename,
            int line_no
        );

        /* Add a command to this breakpoint's command list. */
        void add_command(std::string command);

        /* Remove a command from this breakpoint's command list. */
        bool remove_command(std::string command);

        /* Clear all commands from this breakpoint. */
        void clear_commands();

        Breakpoint& operator=(const Breakpoint& right);
    };

    /* An object for managing a GDB process. */
    class Controller {
        // Module
        static bool is_initialized; // whether the module has been initialized

        /* Maps pids to pointers to Controller instances associated with running
           or stopped GDB processes. On spawn, a Controller is added here.
           On termination (detected in sigchld_handler), a Controller is
           removed from here.

           There should not be any dangling pointers in here, because if the
           client loses a Controller object, ~Controller() will be called,
           which will kill the GDB process, causing sigchld_handler to remove
           the Controller from here. */
        static std::unordered_map<int, Controller*> running_gdbs;
        static void sigchld_handler(int sig_num, siginfo_t *sinfo, void *unused);

        // Communication
        int fd0[2]; // PDB -> GDB
        int fd1[2]; // GDB -> PDB
        FILE* in;

        // Process information
        bool running = false;
        int pid = 0;

        // GDB information
        std::unordered_map<int, Breakpoint> breakpoints; // bp_no : bp

    public:
        Controller();
        ~Controller();

        /* Returns true if this object is associated with a currently
           running GDB process. */
        bool is_running();

        /* Returns the id of the GDB process. */
        int get_pid();

        /* Spawn a GDB process. Does nothing if this
           object is already associated with a running
           GDB process. */
        void spawn(std::string program_name);

        /* Send text to the GDB process.
           Wait for the process to reply with a line that starts with (gdb). */
        std::string send(std::string command);

        /* Send text to the GDB process.
           Wait for the process to reply with a line that starts with
           response_terminator. */
        std::string send(std::string command, std::string response_terminator);

        /* Kill the GDB process associated with this Controller. */
        void exit();

        /* Run the target program. */
        void run();

        /* Continue the target program from a breakpoint. */
        void cont();

        /* Add a breakpoint.
           Returns the newly created breakpoint. */
        Breakpoint& add_breakpoint(std::string filename, unsigned int line_no);

        /* Remove a breakpoint.
           Returns true if the breakpoint existed. */
        bool remove_breakpoint(Breakpoint& bp);

        /* Returns the specified breakpoint. */
        Breakpoint& get_breakpoint(unsigned int bp_no);
    };
}
#endif
