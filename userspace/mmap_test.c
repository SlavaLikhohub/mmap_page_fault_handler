#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define NPAGES 8
#define NODE_PATH "dev/mmap"

/* this is a test program that opens the mmap_drv.
   It reads out values of the kmalloc() and vmalloc()
   allocated areas and checks for correctness.
   You need a device special file to access the driver.
   The device special file is called 'node' and searched
   in the current directory.
   To create it
   - load the driver
     'insmod mmap_mod.o'
   - find the major number assigned to the driver
     'grep mmapdrv /proc/devices'
   - and create the special file (assuming major number 254)
     'mknod node c 254 0'
*/

int main(int argc, char *argv[])
{
        int fd;
        bool fault = false;
        unsigned int *vadr;
        unsigned int *kadr;

        int len = NPAGES * getpagesize();

        if (argc == 2 && strcmp(argv[1], "-f") == 0) {
                fault = true;
                printf("Got fault flag\n");
        }

        if ((fd = open(NODE_PATH, O_RDWR|O_SYNC)) < 0)
        {
                perror("open");
                exit(-1);
        }

        printf("Try to mmap vadr\n");
        vadr = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0);

        if (vadr == MAP_FAILED)
        {
                perror("mmap");
                exit(-1);
        }

        printf("Mmaped fd\n");

        if ((vadr[0] != 0xaffe0000) || (vadr[1] != 0xbeef0000)
            || (vadr[len/sizeof(int) - 2] != (0xaffe0000 + len/sizeof(int) - 2))
            || (vadr[len/sizeof(int) - 1] != (0xbeef0000 + len/sizeof(int) - 2)))
        {
                printf("0x%x 0x%x\n", vadr[0], vadr[1]);
                printf("0x%x 0x%x\n", vadr[len/sizeof(int)-2], vadr[len/sizeof(int)-1]);
        }

        printf("Try to mmap kadr\n");

        kadr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED| MAP_LOCKED, fd, len);

        printf("Mmaped fd\n");

        if (kadr)
        {
                perror("mmap");
                exit(-1);
        }

        printf("Try to read fd\n");

        if ((kadr[0] != 0xdead0000) || (kadr[1] != 0xbeef0000)
            || (kadr[len / sizeof(int) - 2] != (0xdead0000 + len / sizeof(int) - 2))
            || (kadr[len / sizeof(int) - 1] != (0xbeef0000 + len / sizeof(int) - 2)))
        {
                printf("0x%x 0x%x\n", kadr[0], kadr[1]);
                printf("0x%x 0x%x\n", kadr[len / sizeof(int) - 2], kadr[len / sizeof(int) - 1]);
        }

        printf("Done, close fd\n");

        close(fd);
        return(0);
}

