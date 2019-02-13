# Patch the kernel

You can follow the steps below to patch the kernel manually.

#### STEP1. modify include/linux/kvm_host.h
Find the defination of `struct kvm`, and add codes to the end of `struct kvm`:
```
#ifdef CONFIG_KVM_EPT_SAMPLE
    int (*on_ept_sample)(struct kvm* kvm, unsigned long gpa, unsigned long code);
    void* ept_sample_privdata;
#endif
```
#### STEP2. modify arch/x86/kvm/vmx.c
Find the implemention of function

`static int handle_ept_violation(struct kvm_vcpu *vcpu)`,

and add codes after `u64 error_code;` :
```
#ifdef CONFIG_KVM_EPT_SAMPLE
    struct kvm* kvm = vcpu->kvm;
    typeof(kvm->on_ept_sample) on_ept_sample = kvm->on_ept_sample;
    gpa = vmcs_read64(GUEST_PHYSICAL_ADDRESS);
#endif
```
and add codes after `exit_qualification = vmcs_readl(EXIT_QUALIFICATION);` :
```
#ifdef CONFIG_KVM_EPT_SAMPLE
    if(on_ept_sample && on_ept_sample(kvm, (unsigned long)gpa, exit_qualification))
        return 1;
#endif
```
and wrap the original `gpa = vmcs_read64(GUEST_PHYSICAL_ADDRESS);` with `#ifndef CONFIG_KVM_EPT_SAMPLE` and `#endif`.

As a result, this function is like this:
```
static int handle_ept_violation(struct kvm_vcpu *vcpu)
{
    unsigned long exit_qualification;
    gpa_t gpa;
    u64 error_code;
#ifdef CONFIG_KVM_EPT_SAMPLE
    struct kvm* kvm = vcpu->kvm;
    typeof(kvm->on_ept_sample) on_ept_sample = kvm->on_ept_sample;
    gpa = vmcs_read64(GUEST_PHYSICAL_ADDRESS);
#endif

    exit_qualification = vmcs_readl(EXIT_QUALIFICATION);

#ifdef CONFIG_KVM_EPT_SAMPLE
    if(on_ept_sample && on_ept_sample(kvm, (unsigned long)gpa, exit_qualification))
        return 1;
#endif
    /*
     * EPT violation happened while executing iret from NMI,
     * "blocked by NMI" bit has to be set before next VM entry.
     * There are errata that may cause this bit to not be set:
     * AAK134, BY25.
     */
    if (!(to_vmx(vcpu)->idt_vectoring_info & VECTORING_INFO_VALID_MASK) &&
            enable_vnmi &&
            (exit_qualification & INTR_INFO_UNBLOCK_NMI))
        vmcs_set_bits(GUEST_INTERRUPTIBILITY_INFO, GUEST_INTR_STATE_NMI);

#ifndef CONFIG_KVM_EPT_SAMPLE
    gpa = vmcs_read64(GUEST_PHYSICAL_ADDRESS);
#endif
    trace_kvm_page_fault(gpa, exit_qualification);
    //...
```

#### STEP3. modify arch/x86/kvm/Kconfig
Add codes after segment of `config KVM_INTEL` :
```
config KVM_EPT_SAMPLE
     bool "KVM EPT memory access sample support"
     depends on KVM_INTEL && X86_64
     ---help---
       Provides support for memory access sample of a KVM instance based on VT-x.
```

# Enable the feature
After above 3 steps, kernel patch is ready. Now, run `make menuconfig` or `make config` to set `CONFIG_KVM_EPT_SAMPLE=y`.
