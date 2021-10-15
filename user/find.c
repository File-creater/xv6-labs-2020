#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char * filename;

void dfs(char * path) {

    int fd = open(path, 0);
    if (fd < 0) {
        fprintf(2, "find: cann't open %s\n", path);
        return ;
    }

    struct dirent de;
    struct stat st;

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cann't stat %s\n", path);
        close(fd);
        return ;
    }

    if (st.type == T_FILE) {
        fprintf(2, "find: search in a file: %s \n", path);
        close(fd);
        return ;
    }

    // printf("hhh and path is %s\n", path);

    char buf[512], *p;
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            continue;
        }

        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
            printf("find: cann't stat %s\n", buf);
            continue;
        }

        if (st.type == T_DIR) {
            dfs(buf);
        }
        else {
            if (strcmp(de.name, filename) == 0) {
                printf("%s\n", buf);
            }
        }
    }
    
}

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("usage : find <path> <filename>\n");
        exit(0);
    }

    filename = argv[2];

    dfs(argv[1]);

    exit(0);
}