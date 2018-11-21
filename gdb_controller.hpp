#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

class GDBInterface;
class Breakpoint;
class GDBController;

class GDBInterface {
    int fd0[2]; // PDB -> GDB
    int fd1[2]; // GDB -> PDB
    FILE* in;
public:
    GDBInterface();
    GDBInterface(int fd0[2], int fd1[2]);
    bool check_startup_output();
    std::string send(std::string command);
    std::string send(std::string command, std::string response_terminator);
};

class Breakpoint {
    GDBController& controller;
    GDBInterface& interface;

    int id;
    std::string filename;
    int line_no;
    std::vector<std::string> commands;

    /* Update GDB with the new command list. */
    void update_commands();

public:
    Breakpoint(
        GDBController& controller,
        GDBInterface& interface,
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

class GDBController {
    GDBInterface interface;
    bool running = false;
    int pid = 0;
    std::unordered_map<int, Breakpoint> breakpoints; // bp_no : bp

public:
    /* Spawn a GDB process. Does nothing if this
       object is already associated with a running
       GDB process. */
    void spawn(std::string program_name);

    /* Kill the GDB process associated with this GDBController. */
    void kill();

    /* Run the target program. */
    void run();

    /* Continue the target program from a breakpoint. */
    void cont();

    /* Add a breakpoint.
       Returns the number of the newly created breakpoint. */
    Breakpoint& add_breakpoint(std::string filename, unsigned int line_no);

    /* Remove a breakpoint.
       Returns true if the breakpoint existed. */
    bool remove_breakpoint(Breakpoint& bp);

    /* Returns the specified breakpoint. */
    Breakpoint& get_breakpoint(unsigned int bp_no);
};
