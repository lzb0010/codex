# Makefile for i.MX6ULL LED Test Driver
obj-m += led_test.o

# 内核源码路径 - 根据你的实际路径修改
KDIR := /home/huanyu/linux/linux_driver

# 交叉编译工具链
CROSS_COMPILE := arm-linux-gnueabihf-
ARCH := arm

all:
	make -C $(KDIR) M=$(PWD) modules ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	cp led_test.ko /home/huanyu/linux/tftp/
	@echo "已复制到TFTP目录"
