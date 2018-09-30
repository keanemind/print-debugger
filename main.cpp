#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <sys/wait.h>
#include <string>

int main(int argc, char** argv) {
    if (argc == 0) {
        // Print help
    }
    std::cout << "Running GDB." << std::endl;
    int fd0[2]; // PDB -> GDB
    int fd1[2]; // GDB -> PDB
    pipe(fd0);
    pipe(fd1);
    int pid = fork();
    if (pid == -1) {
        std::cout << "Fork failed." << std::endl;
    } else if (pid == 0) {
        // Child
        // Close unused ends of pipes
        close(fd0[1]);
        close(fd1[0]);

        // Set up argument list
        char** gdb_argv = new char*[argc+2];

        gdb_argv[0] = const_cast<char*>("gdb");
        gdb_argv[1] = const_cast<char*>("--interpreter=mi");
        for (int i = 0; i < argc; i++) { // this copies null terminator too
            gdb_argv[2+i] = argv[1+i];
        }
        for (int i = 0; i < argc + 2; i++) {
            std::cout << gdb_argv[i] << " ";
        }
        std::cout << std::endl;

        // Duplicate ends of the pipes to stdin and stdout
        dup2(fd0[0], 0);
        dup2(fd1[1], 1);

        // Run GDB
        int err = execvp("gdb", gdb_argv);
        if (err == -1) {
            std::cout << "execvp failure:" << strerror(errno) << std::endl;
        }
        exit(1);
    }
    // Parent
    // Close unused ends of pipes
    close(fd0[0]);
    close(fd1[1]);

    // Create streams
    FILE* out = fdopen(fd0[1], "w");
    FILE* in = fdopen(fd1[0], "r");

    // Testing
    char gdb_out[2000];
    int bytes_read;

    sleep(2); // this sleep breaks the program - why?
    bytes_read = read(fd1[0], gdb_out, 2000);
    gdb_out[bytes_read] = '\0';
    std::cout << "Bytes read: " << bytes_read << std::endl;
    std::cout << gdb_out << std::endl;

    /*
    write(fd0[1], "run\n", 4);
    read(fd1[0], gdb_out, 2000);
    std::cout << gdb_out << std::endl;
    */

    write(fd0[1], "-exec-run\r\n", 11); // including null terminator causes GDB to not close
    sleep(2);
    bytes_read = read(fd1[0], gdb_out, 2000);
    //fprintf(out, "run\n");
    //fgets(gdb_out, 2000, in);
    gdb_out[bytes_read] = '\0';
    std::cout << "Bytes read: " << bytes_read << std::endl;
    std::cout << gdb_out << std::endl;

    /*
    bytes_read = read(fd1[0], gdb_out, 2000);
    gdb_out[bytes_read] = '\0';
    std::cout << gdb_out << std::endl;
    */

    write(fd0[1], "-gdb-exit\r\n", 11);
    sleep(2);
    bytes_read = read(fd1[0], gdb_out, 2000);
    //fprintf(out, "quit\n");
    //fgets(gdb_out, 2000, in);
    gdb_out[bytes_read] = '\0';
    std::cout << "Bytes read: " << bytes_read << std::endl;
    std::cout << gdb_out << std::endl;

    waitpid(pid, NULL, 0);
    std::cout << "GDB exited." << std::endl;
}
