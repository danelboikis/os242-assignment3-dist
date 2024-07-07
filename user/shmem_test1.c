#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define size 5000

int
main(int argc, char *argv[])
{
    char *mem = malloc(size);
    printf("mem: %p\n", mem);
    // write hello child to mem
    strcpy(mem, "hello child");
    if (mem == 0) {
        printf("malloc failed\n");
        exit(1);
    }
    int src_pid = getpid();

    int pid = fork();
    if (pid == 0) {
        // print sbrk(0)
        printf("child sbrk: %p\n", sbrk(0));

        // child waits for parent to map shared memory
        //sleep(10);
        

        char* s_mem = (char*) map_shared_pages(src_pid, getpid(), (uint64)mem, size);
        if (s_mem == 0) {
            printf("map_shared_pages failed\n");
            exit(1);
        }
        // print whats returned in s_mem
        printf("%p\n", s_mem);
        printf("child reads from mem: %s\n", s_mem);

        printf("%p\n", sbrk(0));
    }
    else {
        // parent writes to mem "hello child"
        //strcpy(mem, "hello child");
        //printf("parent wrote to mem\n");
        // print from mem
        // printf("parent reads from mem: %s\n", mem);
        wait(0);
    }

    exit(0);
}