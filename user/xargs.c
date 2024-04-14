#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

// xargs echo hello 
// 0      1     2
// argc = 3
// command = echo
// args -> echo hello ...
int
main(int argc, char* argv[]) {
    char args[MAXARG][32];
    char command[64];
    strcpy(command, argv[1]);
    int j = 0;
    for (int i = 1; i < argc && j < MAXARG; i++) 
        strcpy(args[j++], argv[i]);
    while (j < MAXARG && gets(args[j], sizeof(args[j]))[0] != '\0') {
        args[j][strlen(args[j]) - 1] = '\0'; // drop CRLF(new line)
        j++;
    }
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    }
    if (pid == 0) {
        // why must use a new char* array exec_args?
        // if use (char**) args;
        // when program use args[0], then args[1], it will arrive args[1] from args[0] by adding sizeof(pointer)
        // it will cause problems

        char* exec_args[MAXARG + 1];
        for (int k = 0; k < j; k++) {
            exec_args[k] = args[k];
        }
        exec(command, exec_args);
        exit(0);
    } else {
        wait(0);
    }
    exit(0);
}