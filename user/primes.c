#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void dfs(int *p) {
    // a new process read from p[0], and write to p2[1], then call fork(), 
    int num;
    if (read(p[0], &num, sizeof(num)) == 0) {
        return ;
    } 

    printf("prime %d\n", num);

    int tmp;

    int p2[2];
    pipe(p2);

    while (read(p[0], &tmp, sizeof(tmp)) != 0) {
        if (tmp % num != 0) {
            write(p2[1], &tmp, sizeof(tmp));
        }
    }

    close(p2[1]);
    close(p[0]);

    if (fork() == 0) {
        dfs(p2);
    }
    else {
        wait(0);
    }
    
}

int main(int argc, char ** argv) {

    int p[2];
    pipe(p);

    for (int i = 2; i < 36; ++i) {
        write(p[1], &i, sizeof(i));
    }

    close(p[1]);

    dfs(p);

    exit(0);
}