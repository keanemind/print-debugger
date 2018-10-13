#include <cstdio>
#include <string>

class GDBController {
    bool running = false;
    int pid = 0;
    int fd0[2]; // PDB -> GDB
    int fd1[2]; // GDB -> PDB
    FILE* in;

    public:
    /* Spawn a GDB process. Does nothing if this
       object is already associated with a running
       GDB process. */
    void spawn(std::string program_name);
    void kill();
    void run();
    void cont();
    void add_breakpoint(std::string filename, unsigned int line_no);
};