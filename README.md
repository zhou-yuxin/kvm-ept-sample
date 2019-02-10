# kvm-ept-sample
In a hybrid memory system, if hot pages reside in DRAM (fast but expensive) and cold pages reside in NVM (slow but cheap), hardware budget will decrease a lot while performance will decrease slighter, thus a higher performance-price ratio will be achieved.

This project is aimed to help adopt hybrid memory in Virtual Machine Cloud on Intel platform. Imagine a physical server is equipped with both DRAM and NVM, and divided into many virtual machines for sell, and A daemon process is detecting the memory access pattern of each VM instance and migrate hot / cold pages to DRAM / NVM periodically. If so, VM customers use hybrid memory transparently, and the VM provider saves money.

The core of this project is a **easy-to-use kernel module** that provides the capability to sample memory accesses of a KVM instance based on Intel VT-x. Although the high-level policy of page temperature estimating and page migration are none of its business, the project as well provides a demo. The demo itself is a simple but useful tool to migrate pages of a VM instance.

#### STEP 1. patch the kernel
The standardized way to patch kernel is using a patch file, but it is dependent on the kernel version. Luckly, my patch to kernel is very simple and independent, see [kernel_patch.md](./kernel_patch.md). 

Then, rebuild the kernel and reboot to use it.

#### STEP 2. build the kernel module
