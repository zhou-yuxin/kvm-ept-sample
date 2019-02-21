obj-m := kvm_ept_sample.o
kvm_ept_sample-objs := main.o interact.o sampler.o queue.o
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.c .*.cmd .cache.mk .tmp_versions Module.symvers modules.order

.PHONY: clean

