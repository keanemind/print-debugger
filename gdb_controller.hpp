#include <cstdio>
#include <string>

class GDBController {
    bool running = false;
    int pid = 0;
    FILE* out;
    FILE* in;

    public:
    /* Spawn a GDB process. Does nothing if this
       object is already associated with a running
       GDB process. */
    void spawn(std::string program_name);
};