#include <cstdio>
#include <string>
#include <vector>

class GDBController {
    bool running = false;
    int pid = 0;
    int fd0[2]; // PDB -> GDB
    int fd1[2]; // GDB -> PDB
    FILE* in;

    void set_breakpoint_command(
        unsigned int bp_no,
        std::vector<std::string> commands
    );

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
    int add_breakpoint(std::string filename, unsigned int line_no);

    /* Make the specified breakpoint print the value of the name
       provided. */
    void set_breakpoint_print(unsigned int bp_no, std::string name);
};