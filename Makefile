obj-m := vcnl4010_i2c.o
ARCH=arm
CROSS_COMPILE=arm-cortex_a8-linux-gnueabihf-
KERNEL_DIR=~/Projects/beaglebone_black/bbb_custom_image/linux-5.4.50

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_DIR) M=$(PWD) clean
help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_DIR) M=$(PWD) help
