obj-m += simplefs.o

simplefs-objs := \
	super.o \
	inode.o \
	file.o \
	dir.o \
	ioctl.o \
	utils.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all kernel userspace clean

all: kernel userspace

kernel:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

userspace:
	$(MAKE) -C userspace

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -C userspace clean