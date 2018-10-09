#include <cstdio>
#include <string>

class GDBController {
    bool running = false;
    int pid = 0;
    FILE* out;
    FILE* in;

    public:
    void spawn(std::string program_name);
};