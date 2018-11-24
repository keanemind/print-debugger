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

    /* An object for managing a GDB process. */
    class Controller {
        /* Module variables */
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

        /* Communication variables */
        int fd0[2]; // pipe: PDB -> GDB
        int fd1[2]; // pipe: GDB -> PDB
        FILE* in;

        /* Process information variables */
        bool running = false;
        int pid = 0;

        static void sigchld_handler(
            int sig_num, siginfo_t *sinfo, void *unused
        );

    public:
        Controller();
        ~Controller();

        /* Return true if this object is associated with a currently
           running GDB process. */
        bool is_running();

        /* Return the id of the GDB process. */
        int get_pid();

        /* Spawn a GDB process. Do nothing if this
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
           Return the newly created breakpoint. */
        Breakpoint add_breakpoint(std::string filename, unsigned int line_no);

        /* Remove a breakpoint. Using the Breakpoint object passed in after
           calling this function is undefined behavior.
           Return true if the breakpoint existed. */
        bool remove_breakpoint(int bp_no);
    };

    /* A GDB breakpoint. */
    class Breakpoint {
        Controller& controller;

        int id; // breakpoint number
        std::string filename;
        int line_no;
        std::vector<std::string> commands;

        Breakpoint(
            Controller& controller,
            int id,
            std::string filename,
            int line_no
        );

        /* Update GDB with the current command list. */
        void update_commands();

    public:
        friend class Controller;

        /* Return the breakpoint's number in GDB. */
        int get_id();

        std::string get_filename();

        int get_line_no();

        /* Add a command to this breakpoint's command list. */
        void add_command(std::string command);

        /* Remove a command from this breakpoint's command list. */
        bool remove_command(std::string command);

        /* Clear all commands from this breakpoint. */
        void clear_commands();

        Breakpoint& operator=(const Breakpoint& right);
    };
}
#endif
