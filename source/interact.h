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

struct interact
{
    struct semaphore file_lock;
    struct sampler sampler;
    struct queue queue;
    spinlock_t queue_lock;
};

struct interact_sample
{
    uint32_t gfn: 29;
    uint32_t xwr: 3;
};

struct interact_get_memslots
{
    struct interact_memslot
    {
        uint64_t gpa;
        uint64_t hva;
        size_t page_count;
    }*
    memslots;
    size_t capacity;
    size_t count;
};

int interact_open(struct inode* inode, struct file* file);

long interact_ioctl(struct file* file, unsigned int cmd, unsigned long arg);

ssize_t interact_read(struct file* file, char* buffer, size_t capacity, loff_t* offset);

int interact_release(struct inode* inode, struct file* file);

#endif
