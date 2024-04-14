#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    int pid;
    // a byte
    char buf[1];
    int p1[2]; // parent read -> p1[0], child write -> p1[1]
    int p2[2]; // child read -> p2[0], parent write -> p2[1]
    // create 2 pair of pipelines
    pipe(p1);
    pipe(p2);
    pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        // child process
        // close write end, only read
        close(p2[1]);
        // get child process pid
        pid = getpid();
        if (read(p2[0], buf, sizeof(buf) <= 0)) {
            fprintf(2, "child process read failed\n");
            exit(1);
        }
        fprintf(1, "%d: received ping\n", pid);
        // close read end
        close(p2[0]);
        // close read end, only write
        close(p1[0]);
        if (write(p1[1], buf, sizeof(buf)) <= 0) {
            fprintf(2, "child process write failed\n");
            exit(1);
        }
        // write down, close write
        close(p1[1]);
        exit(0);
    } else if (pid > 0) {
        // parent process
        // close read end, only write
        close(p2[0]);
        // write a byte to child
        if (write(p2[1], buf, sizeof(buf)) <= 0) {
            fprintf(2, "parent process write failed\n");
            exit(1);
        }
        // close write end
        close(p2[1]);
        // close wirte end, only read
        close(p1[1]);
        if (read(p1[0], buf, sizeof(buf)) <= 0) {
            fprintf(2, "parent process read failed\n");
            exit(1);
        }
        pid = getpid();
        fprintf(1, "%d: received pong\n", pid);
        close(p1[0]);
    }
    exit(0);
}