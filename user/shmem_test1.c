#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define size 8180

int
main(int argc, char *argv[])
{
    char *mem = sbrk(0);
    sbrk(size);
    // write hello child to mem
    strcpy(mem, "hello child");
    
    int src_pid = getpid();

    int pid = fork();
    if (pid == 0) {
        char* s_mem = (char*) map_shared_pages(src_pid, getpid(), (uint64)mem, size);
        if (s_mem == 0) {
            printf("map_shared_pages failed\n");
            exit(1);
        }

        printf("child reads from mem: %s\n", s_mem);
    }
    else {
        wait(0);
    }

    exit(0);
}