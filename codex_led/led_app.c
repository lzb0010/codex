/* led_app.c - 用户层LED测试程序 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define GPIO_EXPORT    "/sys/class/gpio/export"
#define GPIO_DIRECTION "/sys/class/gpio/gpio3/direction"
#define GPIO_VALUE     "/sys/class/gpio/gpio3/value"
#define GPIO_NUM       "3"

int main(int argc, char *argv[])
{
    int fd;
    int led_on = 0;
    
    printf("=== i.MX6ULL LED Test Application ===\n");
    
    /* 1. 导出GPIO */
    fd = open(GPIO_EXPORT, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open GPIO export");
        return -1;
    }
    write(fd, GPIO_NUM, strlen(GPIO_NUM));
    close(fd);
    printf("GPIO %s exported\n", GPIO_NUM);
    
    /* 2. 设置为输出模式 */
    fd = open(GPIO_DIRECTION, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open GPIO direction");
        return -1;
    }
    write(fd, "out", 3);
    close(fd);
    printf("GPIO %s set as output\n", GPIO_NUM);
    
    /* 3. 闪烁LED */
    printf("LED will blink 10 times...\n");
    for (int i = 0; i < 10; i++) {
        led_on = !led_on;
        
        fd = open(GPIO_VALUE, O_WRONLY);
        if (fd < 0) {
            perror("Failed to open GPIO value");
            return -1;
        }
        
        if (led_on) {
            write(fd, "1", 1);
            printf("LED ON\n");
        } else {
            write(fd, "0", 1);
            printf("LED OFF\n");
        }
        close(fd);
        
        sleep(1);
    }
    
    /* 4. 熄灭LED */
    fd = open(GPIO_VALUE, O_WRONLY);
    write(fd, "0", 1);
    close(fd);
    printf("LED test completed!\n");
    
    return 0;
}
