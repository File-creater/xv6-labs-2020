#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int read_line(char * buf) {
    char c;
    if (read(0, &c, sizeof(char)) != 1) {
        return 0;
    }

    *buf++ = c;

    while (read(0, &c, sizeof(char)) != 0 && c != '\n') {
        *buf++ = c;
    }

    *buf++ = 0;

    return 1;
}

int main(int argc, char ** argv) {

    char buf[512];

    while (read_line(buf)) {

        // printf("buf: %s\n", buf);
        if (fork() == 0) {
            char * child_argv[MAXARG];
            int i = 1;
            for (; i < argc; ++i) {
                child_argv[i-1] = argv[i];
            }
            child_argv[i-1] = buf;
            ++i;
            child_argv[i-1] = 0; 

            // for (int j = 0; j < i; ++j) {
            //     printf("child_argv[%d] is %s\n", j, child_argv[j]);
            // }

            exec(child_argv[0], child_argv);

        }
        else {
            wait(0);
        }
    }
    exit(0);
}