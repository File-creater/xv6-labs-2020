#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char ** argv) {

    int p1[2], p2[2];

    if (pipe(p1) < 0 || pipe(p2) < 0) {
        printf("pipe error\n");
        exit(1);
    }

    if (fork() == 0) {
        // child, read from p1, write to p2
        close(p1[1]);
        close(p2[0]);

        char buf;
        if(read(p1[0], &buf, sizeof(buf) < 0)) {
            printf("read error\n");
            exit(1);
        }

        printf("%d: received ping\n", getpid());

        if(write(p2[1], &buf, sizeof(buf) != 1)) {
            printf("write error\n");
            exit(1);
        }

        close(p1[0]);
        close(p2[1]);

        exit(0);
    }
    else {
        char buf = 'a';

        close(p1[0]);
        close(p2[1]);

        if(write(p1[1], &buf, sizeof(buf)) != 1) {
            printf("write error\n");
            exit(1);
        }

        if(read(p2[0], &buf, sizeof(buf)) < 0) {
            printf("read error\n");
            exit(1);
        }

        printf("%d: received pong\n", getpid());

        wait(0);

        close(p1[1]);
        close(p2[0]);

        exit(0);
    }

    exit(0);
}