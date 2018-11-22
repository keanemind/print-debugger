#ifndef GDB_CONTROLLER_HPP
#define GDB_CONTROLLER_HPP 1
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>


namespace GDB {
    class Interface;
    class Breakpoint;
    class Controller;

    /* A messaging interface between this program and a GDB process.
       Must be initialized by a Controller. */
    class Interface {
        int fd0[2]; // PDB -> GDB
        int fd1[2]; // GDB -> PDB
        FILE* in;
    public:
        Interface();
        Interface(int fd0[2], int fd1[2]);

        /* Wait for and receive the output that GDB prints on startup.
           Returns whether the output is what is expected from a successful
           startup. */
        bool check_startup_output();

        /* Send text to the GDB process.
           Wait for the process to reply with a line that starts with (gdb) */
        std::string send(std::string command);

        /* Send text to the GDB process.
           Wait for the process to reply with a line that starts with
           response_terminator. */
        std::string send(std::string command, std::string response_terminator);
    };

    /* A GDB breakpoint. */
    class Breakpoint {
        Controller& controller;
        Interface& interface;

        int id; // breakpoint number
        std::string filename;
        int line_no;
        std::vector<std::string> commands;

        /* Update GDB with the current command list. */
        void update_commands();

    public:
        Breakpoint(
            Controller& controller,
            Interface& interface,
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
        Interface interface;
        bool running = false;
        int pid = 0;
        std::unordered_map<int, Breakpoint> breakpoints; // bp_no : bp

    public:
        /* Spawn a GDB process. Does nothing if this
        object is already associated with a running
        GDB process. */
        void spawn(std::string program_name);

        /* Kill the GDB process associated with this Controller. */
        void kill();

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
