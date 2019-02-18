# kvm-ept-sample
In a hybrid memory system, if hot pages reside in DRAM (fast but expensive) and cold pages reside in NVM (slow but cheap), hardware budget will decrease a lot while performance will decrease slighter, thus a higher performance-price ratio will be achieved.

This project is aimed to help adopt hybrid memory in Virtual Machine Cloud on Intel platform. Imagine a physical server is equipped with both DRAM and NVM, and divided into many virtual machines for sell, and a daemon process is detecting the memory access pattern of each VM instance and migrate hot / cold pages to DRAM / NVM periodically. If so, VM customers use hybrid memory transparently, and the VM provider saves money.

The core of this project is a **easy-to-use kernel module** that provides the capability to sample memory accesses of a KVM instance based on Intel VT-x. Although the high-level policy of page temperature estimating and page migration are none of its business, the project as well provides a demo. The demo itself is a simple but useful tool to migrate pages of a VM instance.

## How to load kvm-ept-sample
Only two steps are required.

#### STEP 1. patch the kernel
The standardized way to patch kernel is using a patch file, but it is dependent on the kernel version. Luckly, my patch to kernel is very simple and independent, see [kernel_patch.md](./kernel_patch.md).

Then, rebuild the kernel and reboot to use it.

#### STEP 2. build the kernel module
Get into the directory *source*, simply run `make`, then a kernel module named *kvm_ept_sample.ko* is created, which is the core of this project.

Then, run `sudo insmod kvm_ept_sample.ko` to load it. You cound see */proc/kvm_ept_sample* if all is ok.

## How to use kvm-ept-sample
A userspace program can open */proc/kvm_ept_sample* and call ioctl() to configure the arguments or get some information of sampling, and call read() to read the samples.

The generic usage is like this (all error detections are omitted):
```
int fd = open("/proc/kvm_etp_sample", O_RDWR);  // open the module
ioctl(fd, KVM_EPT_SAMPLE_CMD_INIT, pid);        // set the pid to sample
ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_PROT, xwr);    // set the access type to sample
ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_FREQ, freq);   // set the frequency to sample
while(1)
{
    struct kvm_ept_sample samples[64];
    ssize_t len = read(fd, samples, sizeof(samples));   // try to read the samples
    if(len)
    {
        size_t count = len / sizeof(struct kvm_ept_sample);
        for(int i = 0; i < count; i++)          // print all the samples
        {
            struct kvm_ept_sample* sample = samples + i;
            printf("gfn = %x, wxr = %x\n", sample->gfn, sample->xwr);
        }
    }
    else
        sleep(1);   // wait for more samples
}
```

As the above code shows, the initialization is consist of 4 steps:
1. Firstly, you should `open("/proc/kvm_ept_sample", O_RDWR)` to get a file descriptor.
2. Then, call `ioctl(fd, KVM_EPT_SAMPLE_CMD_INIT, pid)` to tell it which QEMU-KVM process you want to sample.
3. Similarly, use `ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_PROT, xwr)` to tell it which type of memory access you want to sample. The *xwr* is an 'or'-bits, where 'x' means the 'fetch instruction', 'w' means 'write' and 'r' means 'read'. For example, you want to sample 'fetch instruction' and 'write' but no 'read', you can set *xwr* to 110b, where 'x' = 1, 'w' = 1 and 'r' = 0.
4. The last step of initialization is to set the sample frequency, in Hz. A non-zero frequency will start sampling, while a zero will stop it.

All above `ioctl()`(s) return 0 if OK, or an error code if something is wrong. The *xwr* and *freq* are OK to be adjusted in runtime.

#### DEMO 1. print_samples: sample the memory accesses of a given QEMU-KVM instance and print them
This demo is the simplest one to show how to use the APIs provided by kvm-ept-sample. To build it, go into *demo* directory, and run `make print_samples`. Run `print_samples <pid>` to lanuch it, where <pid> is the the PID of a QEMU-KVM process.

