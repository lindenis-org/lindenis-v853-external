/* author: allwinner:AW1742
 * cat /sys/kernel/debug/pinctrl/pio/pins get the pgio_num
 * pin 0 (PA0) 
 * pin 1 (PA1) 
 * pin 2 (PA2) 
 * ......
 * pin 145 (PE17)
 * 
 * usage : PE17 High-level LED lights up
 * ledtester 145 1
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define TAG "ledtester"
#include <dragonboard/dragonboard.h>
#define GPIO_DEV_EXPORT		"/sys/class/gpio/export"
#define GPIO_DEV_UNEXPORT	"/sys/class/gpio/unexport"

static int check_system_status(pid_t status)
{
	
    if (WIFEXITED(status))
    {
        if (0 == WEXITSTATUS(status))
        {
      //      printf("run success\n");
        }
        else
        {
            printf("run fail, exit code %d\n", WEXITSTATUS(status));
            return -1;
        }
    }
    else
    {
        printf("exit code %d\n", WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    	int fifoFd = 0;
	int gpio_fd = 0;
	char gpio_exist[64];
	char export_gpio[64];
	char unexport_gpio[64];
	char gpio_direction[64];
	char gpio_value[64];
	unsigned int gpio_num;
	int light_value = -1;
	int count = 0;
	
    	char str_pass[] = "P[LED] PASS";
    	char str_fail[] = "F[LED]:FAIL";
    	char str_gpio_fail[] = "W[LED] open gpio_file fail ";
    	pid_t status;
	printf("===== led test ======\n");
	if (argc == 1) {
		printf("not input gpio number\n");
		return -1;
	} else if (argc == 2) {
		gpio_num = atoi(argv[1]);
		printf("gpio_num: %d\n", gpio_num);
		printf("default 1 led light\n");
	} else if ( argc == 3) {
		gpio_num = atoi(argv[1]);
		light_value = atoi(argv[2]);
		printf("gpio_num: %d, light_value: %d\n", gpio_num, light_value);
	} else {
		printf(" Too many input parameters \n");
		return -1;
	}
	printf("open FIFO_LED_DEV start\n");
	if ((fifoFd = open(FIFO_LED_DEV, O_WRONLY | O_CREAT)) < 0)
    	{
        	if (mkfifo(FIFO_LED_DEV, 0666) < 0)
        	{
                    printf("mkfifo failed(%s)\n", strerror(errno));
            		return -1;
        	}else
        	{
            		fifoFd = open(FIFO_LED_DEV, O_WRONLY);
        	}
    	}
	gpio_fd = open(GPIO_DEV_EXPORT, O_WRONLY);
	if (gpio_fd < 0) {
		printf(" open /sys/class/gpio/export error\n");
		write(fifoFd, str_gpio_fail, strlen(str_gpio_fail));
		close(fifoFd);
		return -1;
	} 
	
	sprintf(gpio_exist, "/sys/class/gpio/gpio%d/direction", gpio_num);
	if (open(gpio_exist, O_WRONLY) >= 0 ) {
		printf("gpio%d is exist\n", gpio_num);
		goto contrl_led;
	}
	
	sprintf(export_gpio, "echo %d > %s", gpio_num, GPIO_DEV_EXPORT);
	status = system(export_gpio);
	if (check_system_status(status)) {
		printf("%s fail %d\n", export_gpio, WEXITSTATUS(status));
		write(fifoFd, str_fail, strlen(str_fail));
		goto error;
	}
	goto contrl_led;


contrl_led:	
	sprintf(gpio_direction, "echo out > /sys/class/gpio/gpio%d/direction", gpio_num);
	printf("%s\n", gpio_direction);
		status = system(gpio_direction);
	if (check_system_status(status)) {
		printf("%s fail %d\n", gpio_direction, WEXITSTATUS(status));
		write(fifoFd, str_fail, strlen(str_fail));
		goto error;
	}
	while(1)
	{
		if ( light_value == -1) {
			sprintf(gpio_value, "echo 1 > /sys/class/gpio/gpio%d/value", gpio_num);
			status = system(gpio_value);
			if (check_system_status(status)) {
				printf("%s fail %d\n", gpio_value, WEXITSTATUS(status));
				write(fifoFd, str_fail, strlen(str_fail));
				goto error;
			}
			usleep(200 * 1000);
			count++;
			sprintf(gpio_value, "echo 0 > /sys/class/gpio/gpio%d/value", gpio_num);
			status = system(gpio_value);
			if (check_system_status(status)) {
				printf("%s fail %d\n", gpio_value, WEXITSTATUS(status));
				write(fifoFd, str_fail, strlen(str_fail));
				goto error;
			}
			usleep(200 * 1000);
			if (count >= 10) {
				sprintf(str_pass, "P[RTC] test times:5%d", count);
				write(fifoFd, str_pass, strlen(str_pass));
				goto exit;
			}	
			
		} else {
			sprintf(gpio_value, "echo %d > /sys/class/gpio/gpio%d/value", light_value, gpio_num);
			status = system(gpio_value);
			if (check_system_status(status)) {
				printf("%s fail %d\n", gpio_value, WEXITSTATUS(status));
				write(fifoFd, str_fail, strlen(str_fail));
				goto error;
			}
			usleep(200 * 1000);
			count++;
			sprintf(gpio_value, "echo %d > /sys/class/gpio/gpio%d/value",(!light_value), gpio_num);
			status = system(gpio_value);
			if (check_system_status(status)) {
				printf("%s fail %d\n", gpio_value, WEXITSTATUS(status));
				write(fifoFd, str_fail, strlen(str_fail));
				goto error;
			}
			usleep(200 * 1000);
			if (count >= 10) {
				write(fifoFd, str_pass, strlen(str_pass));
				printf("test FIFO_LED_DEV finish\n");
				goto exit;
			}	
		}		
	}
	
error:
	sprintf(unexport_gpio, "echo %d > %s", gpio_num, GPIO_DEV_UNEXPORT);
	system(unexport_gpio);
	close(fifoFd);
	close(gpio_fd);
	return -1;
	
exit:	
	sprintf(unexport_gpio, "echo %d > %s", gpio_num, GPIO_DEV_UNEXPORT);
	system(unexport_gpio);
    sleep(3);
	close(gpio_fd);
	close(fifoFd);
    return 0;
}
