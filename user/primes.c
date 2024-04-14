#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// each process's work
int 
processWork(int pa_read_fd, int pa_write_fd) {
    // dno't need write end of parent's pipe
    close(pa_write_fd);
    int firstNum;
    int num;
    // to get the first number 
    if (read(pa_read_fd, &firstNum, sizeof(firstNum)) <= 0) {
        close(pa_read_fd);
        exit(1);
    }
    fprintf(1, "prime %d\n", firstNum);

    int p[2];
    pipe(p);
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        processWork(p[0], p[1]);
        exit(0);
    } else {
        close(p[0]);
        while (read(pa_read_fd, &num, sizeof(num)) > 0) {
            // cur proess don't know whether num is prime, write to the next process to judge
            if (num % firstNum != 0)
                write(p[1], &num, sizeof(num));
        }
        close(p[1]);
        close(pa_read_fd);
        wait(0); // must wait for child process to finish
        exit(0);
    }
    return 0;
}

int
main(int argc, char* argv[]) {
    int p[2]; // p[0] read, p[1] write
    pipe(p);
    int number = -1;
    int pid = fork();
    // create pipe
    if (pid < 0) {
        fprintf(2, "initial fork failed\n");
        close(p[0]);
        close(p[1]);
        exit(1);
    }
    if (pid > 0) {
        // main process
        close(p[0]); // main process only can write
        for (int i = 2; i <= 35; i++) {
            number = i;
            if (write(p[1], &number, sizeof(number)) <= 0) {
                fprintf(2, "main process write number to judge error\n");
                break;
            }
        }
        close(p[1]); // main process write down
        wait(0); // must wait for child process to finish
    } else {
        processWork(p[0], p[1]);
    }
    return 0;
}