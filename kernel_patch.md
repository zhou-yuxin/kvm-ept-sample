# Patch the kernel

You can follow the steps below to patch the kernel manually.

#### STEP1. include/linux/kvm_host.h
Find the defination of `struct kvm`, and add lines below to the end of `struct kvm`:
```
#ifdef CONFIG_KVM_EPT_SAMPLE
    int (*on_ept_sample)(struct kvm* kvm, unsigned long gpa, unsigned long code);
    void* ept_sample_privdata;
#endif
```
