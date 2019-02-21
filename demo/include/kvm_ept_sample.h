#ifndef KVM_EPT_SAMPLE_H
#define KVM_EPT_SAMPLE_H

#define KVM_EPT_SAMPLE_PATH             "/proc/kvm_ept_sample"

#define KVM_EPT_SAMPLE_CMD_INIT         1200
#define KVM_EPT_SAMPLE_CMD_SET_PROT     1201
#define KVM_EPT_SAMPLE_CMD_SET_FREQ     1202
#define KVM_EPT_SAMPLE_CMD_GET_MEMSLOTS 1203
#define KVM_EPT_SAMPLE_CMD_DEINIT       1204

#include <stdint.h>

// the structure of a sample
struct kvm_ept_sample_sample
{
    uint32_t gfn: 29;   // the Guest Physical Page Frame Number
    uint32_t xwr: 3;    // the 'or' bits of access type
};

// the structure of a memslot
// Guest Physical Address (GPA) is mapped to Host Virtual Addess (HVA) by 'memory slots'
// For example, a kvm instance has 3 memory slots:
// |    GPA     |         HVA      |  pages  |
// |         0  |  0x7f0000000000  |   1000  |
// |  0x400000  |  0x7f0000800000  |  32768  |
// |  0x900000  |  0x7f0001200000  |    160  |
// so GPA = 0x412345 is in the second slot, and mapped to HVA = 0x7f0000812345
struct kvm_ept_sample_get_memslots
{
    struct kvm_ept_sample_memslot
    {
        uint64_t gpa;       // the base GPA
        uint64_t hva;       // the base HVA
        size_t page_count;  // count of pages in the slot
    }*
    memslots;           // the array of slots
    size_t capacity;    // the max count of the array
    size_t count;       // the actual count of the array
};

#endif
