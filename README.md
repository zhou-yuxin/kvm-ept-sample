# kvm-ept-sample
In a hybrid memory system, if hot pages reside in DRAM (fast but expensive) and cold pages reside in NVM (slow but cheap), hardware budget will decrease a lot while performance will decrease slighter, thus a higher performance-price ratio will be achieved.

This project is aimed to help adopt hybrid memory in Virtual Machine Cloud on Intel platform. Imagine a physical server is equipped with both DRAM and NVM, and divided into many virtual machines for sell, and a daemon process is detecting the memory access pattern of each VM instance and migrate hot / cold pages to DRAM / NVM periodically. If so, VM customers use hybrid memory transparently, and the VM provider saves money.

The core of this project is a **easy-to-use kernel module** that provides the capability to sample memory accesses of a KVM instance based on Intel VT-x. Although the high-level policy of page temperature estimating and page migration are none of its business, the project as well provides a demo. The demo itself is a simple but useful tool to migrate pages of a VM instance.

## How to load kvm-ept-sample
Only two steps are required.

#### STEP 1. patch the kernel
The standardized way to patch kernel is using a patch file, but it is dependent on the kernel version. Luckly, my patch to kernel is very simple and independent, see [kernel_patch/README.md](./kernel_patch/README.md). 

Then, rebuild the kernel and reboot to use it.

#### STEP 2. build the kernel module
Get into the directory *source*, simply run `make`, then a kernel module named *kvm_ept_sample.ko* is created, which is the core of this project.

Then, run `sudo insmod kvm_ept_sample.ko` to load it. You cound see */proc/kvm_ept_sample* if all is ok.

## How to use kvm-ept-sample
A userspace program can open */proc/kvm_ept_sample* and call ioctl() to configure the arguments or get some information of sampling, and call read() to read the samples.

#### DEMO 1. sample the memory accesses of a given QEMU-KVM instance and print them
This demo is the simplest one to show how to use the APIs provided by kvm-ept-sample.
