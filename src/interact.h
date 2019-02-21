#ifndef INTERACT_H
#define INTERACT_H

#include "queue.h"
#include "sampler.h"

#include <linux/fs.h>

#define INTERACT_CMD_INIT           1200
#define INTERACT_CMD_SET_PROT       1201
#define INTERACT_CMD_SET_FREQ       1202
#define INTERACT_CMD_GET_MEMSLOTS   1203
#define INTERACT_CMD_DEINIT         1204

#define INTERACT_MAX_BUFFERED_SAMPLES   65536

// the structure that a file->private_data points to
struct interact
{
    struct semaphore file_lock; // make sure file operations are sequential
    struct sampler sampler;     // core sampler
    struct queue queue;         // queue to buffer samples
    spinlock_t queue_lock;      // protect 'queue'
};

// the structure of a access sample
struct interact_sample
{
    uint32_t gfn: 29;   // the Guest Physical Page Frame Number
    uint32_t xwr: 3;    // the 'or' bits of access type
};

// the argument of GET_MEMSLOTS command
// Guest Physical Address (GPA) is mapped to Host Virtual Addess (HVA) by 'memory slots'
// For example, a kvm instance has 3 memory slots:
// |    GPA     |         HVA      |  pages  |
// |         0  |  0x7f0000000000  |   1000  |
// |  0x400000  |  0x7f0000800000  |  32768  |
// |  0x900000  |  0x7f0001200000  |    160  |
// so GPA = 0x412345 is in the second slot, and mapped to HVA = 0x7f0000812345
struct interact_get_memslots
{
    struct interact_memslot
    {
        uint64_t gpa;       // the base GPA
        uint64_t hva;       // the base HVA
        size_t page_count;  // count of pages in the slot
    }*
    memslots;               // the array of slots (output)
    size_t capacity;        // the max count of the array (input)
    size_t count;           // the actual count of the array (output)
};

int interact_open(struct inode* inode, struct file* file);

long interact_ioctl(struct file* file, unsigned int cmd, unsigned long arg);

ssize_t interact_read(struct file* file, char* buffer, size_t capacity, loff_t* offset);

int interact_release(struct inode* inode, struct file* file);

#endif
