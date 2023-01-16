#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <linux/fs.h>

#define TAG "nandtester"
#include <dragonboard/dragonboard.h>

#define NAND_FLASH "/dev/nand0"

/* just define an ioctl cmd for nand test*/
#define DRAGON_BOARD_TEST    _IO('V',55)

#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)

int main(int argc, char **argv)
{
    int retVal, fifoFd;
    char str[128] = {0};
    char str_pass[] = "P[NAND] PASS";
    char str_fail[] = "F[NAND]:FAIL";

    sleep(2);

    if ((fifoFd = open(FIFO_NAND_DEV, O_WRONLY)) < 0)
    {
        if (mkfifo(FIFO_NAND_DEV, O_WRONLY | O_CREAT) < 0)
        {
            printf("mkfifo failed(%s)\n", strerror(errno));
            return -1;
        }
        else
        {
            fifoFd = open(FIFO_NAND_DEV, O_WRONLY);
        }
    }

    if(FILE_EXIST(NAND_FLASH)){
        printf("%s exit", NAND_FLASH);
        write(fifoFd,str_pass,strlen(str_pass));
    }else{
        printf("%s no exit", NAND_FLASH);
        write(fifoFd,str_fail,strlen(str_fail));
    }

    system("cat /proc/cmdline");

#if 0
    int nandFd = open(NAND_FLASH, O_RDWR);
    if (nandFd < 0) {
        printf("can't open %s(%s)\n", NAND_FLASH, strerror(errno));
        retVal = -1;
    }

    /* if nand ok,return 0; otherwise,return -1 */
    retVal = ioctl(nandFd, DRAGON_BOARD_TEST);
    if (retVal < 0) {
        printf("error in ioctl(%s)......\n", strerror(errno));
        retVal = -1;
    } else {
        printf("nand flash test success\n");
    }

    close(nandFd);
#endif

    close(fifoFd);

    return retVal;
}
