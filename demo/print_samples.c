#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define CMD_INIT            1200
#define CMD_SET_PROT        1201
#define CMD_SET_FREQ        1202
#define CMD_GET_MEMSLOTS    1203
#define CMD_DEINIT          1204

// the structure of a sample
struct sample
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
struct get_memslots
{
    struct memslot
    {
        uint64_t gpa;       // the base GPA
        uint64_t hva;       // the base HVA
        size_t page_count;  // count of pages in the slot
    }*
    memslots;           // the array of slots
    size_t capacity;    // the max count of the array
    size_t count;       // the actual count of the array
};

int main(int argc, char* argv[])
{
    pid_t pid;
    if(argc != 2 || sscanf(argv[1], "%d", &pid) != 1)
    {
        printf("USAGE: %s <pid>\n", argv[0]);
        return 1;
    }
    int fd = open("/proc/kvm_ept_sample", O_RDWR);
    if(fd < 0)
    {
        perror("open() failed");
        return 1;
    }
    if(ioctl(fd, CMD_INIT, pid) < 0)    // set pid
    {
        perror("ioctl() failed");
        return 1;
    }
    if(ioctl(fd, CMD_SET_PROT, 2) < 0)  // set prot, xwr = 2 means only sampleing 'write'
    {
        perror("ioctl() failed");
        return 1;
    }
    if(ioctl(fd, CMD_SET_FREQ, 10000) < 0)  // sample at 10000 Hz
    {
        perror("ioctl() failed");
        return 1;
    }
    struct memslot memslots[64];
    struct get_memslots get_memslots = 
    {
        .memslots = memslots,
        .capacity = 64,
        .count = 0,
    };
    if(ioctl(fd, CMD_GET_MEMSLOTS, &get_memslots) < 0)  // get all memory slots
    {
        perror("ioctl() failed");
        return 1;
    }
    for(size_t i = 0; i < get_memslots.count; i++)
        printf("memslot[%lu]: gpa: %lx, hva: %lx, count: %lu\n",
            i, memslots[i].gpa, memslots[i].hva, memslots[i].page_count);
    while(1)
    {
        struct sample samples[128];
        ssize_t len = read(fd, samples, sizeof(samples));
        if(len < 0)
        {
            perror("read() failed");
            return 1;
        }
        else if(len == 0)
        {
            sleep(1);
            continue;
        }
        assert(len % sizeof(struct sample) == 0);
        size_t count = len / sizeof(struct sample);
        for(size_t i = 0; i < count; i++)
        {
            struct sample* sample = samples + i;
            uint32_t gfn = sample->gfn;
            uint32_t xwr = sample->xwr;
            printf("%6x, %c%c%c\n",
                gfn,
                xwr & 1 ? 'r' : '-',
                xwr & 2 ? 'w' : '-',
                xwr & 4 ? 'x' : '-');
        }
    }
    return 0;
}
