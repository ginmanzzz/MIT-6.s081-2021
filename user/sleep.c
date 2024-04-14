#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(2, "Usage: sleep time\n");
        exit(1);
    }
    if (sleep(atoi(argv[1])) < 0) {
        fprintf(2, "Sleep failed\n");
        exit(1);
    }
    exit(0);
}