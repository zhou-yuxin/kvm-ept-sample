#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

// a node in queue
struct queue_node
{
    struct queue_node* next;    // next node
    size_t head;    // the head position in this node
    size_t tail;    // the tail position in this node
};

struct queue
{
    // constants, keep unchange after init
    size_t entry_size;      // size of user entry in this queue
    size_t entry_per_page;  // how many entries in a node (one page is one node)
    void* (*func_alloc_page)(void* privdata);   // the function to allocate a page
    void (*func_free_page)(void* page, void* privdata); // the function to free a page
    void* privdata;         // the privdata to the two functions above

    struct queue_node* first;   // the first node
    struct queue_node* last;    // the last node

    // statistics
    size_t length;      // how many entries in the queue
    size_t page_count;  // how many pages used
};

// init
//      entry_size: size of of user's element, sizeof(struct user_ele)
//      page_size: size of a memory page, usually 4096
//      func_alloc_page: the function to allocate a page
//      func_free_page: the function to free a page
//      privdata: user's privdata to the two functions above
// return 0 if succeed, or error code if failed.
int queue_init(struct queue* queue, size_t entry_size, size_t page_size,
    void* (*func_alloc_page)(void* privdata),
    void (*func_free_page)(void* page, void* privdata),
    void* privdata);

// add an entry to the tail of the queue
// return the pointer of the new entry, or NULL if no memory
void* queue_add(struct queue* queue);

// take an entry from the head of the queue
// return the pointer of the poped entry, or NULL if the queue is empty
void* queue_take(struct queue* queue);

// glance at the head of the queue, but not remove it
// return the pointer of the first entry, or NULL if the queue is empty
void* queue_glance(struct queue* queue);

// release the resources
//      destructor: the destructor to destroy every element, or NULL
void queue_deinit(struct queue* queue, void (*destructor)(void* value));

#endif

