# SPDX-License-Identifier: GPL-2.0
#
obj-m += ktest.o
MY_CFLAGS += -g -DDEBUG -fanalyzer
ccflags-y += ${MY_CFLAGS}
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo insmod ktest.ko
	sudo rmmod ktest.ko
	sudo dmesg | tail -n 10
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
