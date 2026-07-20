# i.MX6ULL LED 测试指南

## 文件清单
- led_test.c    : 内核驱动模块（LED闪烁）
- led_app.c     : 用户层测试程序
- Makefile      : 编译脚本

## 步骤1：将文件复制到虚拟机

在Windows上，将以下文件复制到虚拟机的 /home/huanyu/linux/linux_driver/led_test/ 目录：
- led_test.c
- led_app.c
- Makefile

可以使用以下方法：
1. 使用WinSCP/FlashFXP等FTP工具
2. 使用共享文件夹
3. 使用scp命令（如果配置了SSH免密）

## 步骤2：在虚拟机上编译

```bash
# 进入目录
cd /home/huanyu/linux/linux_driver/led_test/

# 编译驱动模块
make

# 编译用户层程序
arm-linux-gnueabihf-gcc -o led_app led_app.c -static

# 复制到TFTP目录
cp led_test.ko /home/huanyu/linux/tftp/
cp led_app /home/huanyu/linux/tftp/
```

## 步骤3：在开发板上测试

### 方法A：加载内核驱动（LED自动闪烁）
```bash
# 通过TFTP下载文件
tftp -g -r led_test.ko 192.168.3.66

# 加载驱动
insmod led_test.ko

# 查看内核日志
dmesg | tail

# 卸载驱动
rmmod led_test
```

### 方法B：运行用户层程序（手动控制）
```bash
# 通过TFTP下载文件
tftp -g -r led_app 192.168.3.66

# 添加执行权限
chmod +x led_app

# 运行程序
./led_app
```

## GPIO说明
当前代码使用 GPIO3，请根据你的开发板原理图修改：
- 正点原子开发板：通常LED连接到 GPIO3_IO04 (GPIO_4)
- 野火开发板：通常LED连接到 GPIO1_IO03 或 GPIO5_IO03

修改 led_test.c 中的：
#define LED_GPIO  3  // 改为实际的GPIO编号

## 常见问题

### 1. GPIO编号计算
GPIO编号 = (GPIO组号 - 1) * 32 + 组内偏移
例如：GPIO3_IO04 = (3-1)*32 + 4 = 68

### 2. 查看可用GPIO
```bash
cat /sys/kernel/debug/gpio
```

### 3. 手动控制GPIO
```bash
echo 68 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio68/direction
echo 1 > /sys/class/gpio/gpio68/value  # LED亮
echo 0 > /sys/class/gpio/gpio68/value  # LED灭
```
