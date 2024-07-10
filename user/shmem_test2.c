#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define size 8180

int
main(int argc, char *argv[])
{
    char *mem = sbrk(0);
    sbrk(size);

    int src_pid = getpid();

    int pid = fork();
    if (pid == 0) {
        printf("before map: %d\n", sbrk(0));
        char* s_mem = (char*) map_shared_pages(src_pid, getpid(), (uint64)mem, size);
        if (s_mem == 0) {
            printf("map_shared_pages failed\n");
            exit(1);
        }
        printf("after map: %d\n", sbrk(0));

        // write hello daddy
        strcpy(s_mem, "hello daddy");

        

        unmap_shared_pages(getpid(), (uint64)s_mem, size);
        printf("after unmap: %d\n", sbrk(0));

        // malloc
        char* s_mem2 = (char*) malloc(size);
        if (s_mem2 == 0) {
            printf("malloc failed\n");
            exit(1);
        }
        printf("after malloc: %d\n", sbrk(0));
    }
    else {
        wait(0);

        // read from mem
        printf("parent reads from mem: %s\n", mem);
    }

    return 0;;
}
