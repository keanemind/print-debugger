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
    void kill();
    void run();
    void cont();
    void add_breakpoint(std::string filename, unsigned int line_no);
    void set_breakpoint_print(unsigned int bp_no, std::string name);
};