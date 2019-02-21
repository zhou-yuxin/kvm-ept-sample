#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "kvm_ept_sample.h"

int main(int argc, char* argv[])
{
    pid_t pid;
    if(argc != 2 || sscanf(argv[1], "%d", &pid) != 1)
    {
        printf("USAGE: %s <pid>\n", argv[0]);
        return 1;
    }
    int fd = open(KVM_EPT_SAMPLE_PATH, O_RDWR);
    if(fd < 0)
    {
        perror("open() failed");
        return 1;
    }
    // set pid
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_INIT, pid) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    // set prot, xwr = 6 means sampleing 'exec' and 'write'
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_PROT, 6) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    // sample at 10000 Hz
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_FREQ, 10000) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    struct kvm_ept_sample_memslot memslots[64];
    struct kvm_ept_sample_get_memslots get_memslots = 
    {
        .memslots = memslots,
        .capacity = 64,
        .count = 0,
    };
    // get all memory slots
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_GET_MEMSLOTS, &get_memslots) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    // print memory slots
    for(size_t i = 0; i < get_memslots.count; i++)
        printf("memslot[%lu]: gpa: %lx, hva: %lx, count: %lu\n",
            i, memslots[i].gpa, memslots[i].hva, memslots[i].page_count);
    // read and print samples
    while(1)
    {
        struct kvm_ept_sample_sample samples[128];
        ssize_t len = read(fd, samples, sizeof(samples));
        if(len < 0)
        {
            perror("read() failed");
            return 1;
        }
        else if(len == 0)
        {
            usleep(10000);  // try again later
            continue;
        }
        assert(len % sizeof(struct kvm_ept_sample_sample) == 0);
        size_t count = len / sizeof(struct kvm_ept_sample_sample);  // count of samples
        for(size_t i = 0; i < count; i++)
        {
            uint32_t gfn = samples[i].gfn;
            uint32_t xwr = samples[i].xwr;
            printf("%6x, %c%c%c\n",
                gfn,
                xwr & 1 ? 'r' : '-',
                xwr & 2 ? 'w' : '-',
                xwr & 4 ? 'x' : '-');
        }
    }
    return 0;
}
