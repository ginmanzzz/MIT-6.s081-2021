#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int cmpFile(const char* f1, const char* f2) {
    // f1 is the file to be compared, f2 is the file to search
    while (*f1++) {
        if (strcmp(f1, f2) == 0)
            return 0;
    }
    return 1;
}

void _find(const char* path, const char* filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        return;
    }

    switch (st.type) {
    case T_FILE:
        fprintf(2, "Usage: find <directory> <filename>\n");
        fprintf(2, "input <directory> isnot a directory\n");
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(path);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            // get file or directory
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
                fprintf(2, "find: cannot stat %s\n", path);
                continue;
            }
            if (st.type == T_DIR) {
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                _find(buf, filename);
            } else if (st.type == T_FILE) {
                if (strcmp(de.name, filename) == 0)
                    printf("%s\n", buf);
            }
        }
        break;
    }
    close(fd);
}


int
main(int argc, char* argv[]) {
    // use method: find (directory) (filename)
    if (argc < 3) {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(1);
    }
    _find(argv[1], argv[2]);
    exit(0);
}