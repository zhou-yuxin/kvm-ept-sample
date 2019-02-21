#include "queue.h"

int queue_init(struct queue* queue, size_t entry_size, size_t page_size,
    void* (*func_alloc_page)(void* privdata),
    void (*func_free_page)(void* page, void* privdata),
    void* privdata)
{
    struct queue_node* node;
    assert(queue);
    if(unlikely(!(queue->entry_size = entry_size)))
        ERROR0(-EINVAL, "param <entry_size = 0> is not allowed");
    if(unlikely(page_size < sizeof(struct queue_node) + entry_size))
        ERROR1(-EINVAL, "param <page_size = %lu> is too small", page_size);
    queue->entry_per_page = (page_size - sizeof(struct queue_node)) / entry_size;
    if(unlikely(!(queue->func_alloc_page = func_alloc_page)))
        ERROR0(-EINVAL, "param <func_alloc_page = NULL> is not allowed");
    if(unlikely(!(queue->func_free_page = func_free_page)))
        ERROR0(-EINVAL, "param <func_free_page = NULL> is not allowed");
    queue->privdata = privdata;
    if(unlikely(!(node = func_alloc_page(privdata))))
        ERROR0(-ENOMEM, "func_alloc_page(privdata) failed");
    node->next = NULL;
    node->head = 0;
    node->tail = 0;
    queue->first = queue->last = node;
    queue->length = 0;
    queue->page_count = 1;
    return 0;
}

void* queue_add(struct queue* queue)
{
    struct queue_node* last;
    void* entry;
    assert(queue);
    last = queue->last;
    assert(last);
    if(likely(last->tail < queue->entry_per_page))
        entry = (char*)last + sizeof(struct queue_node) + queue->entry_size * last->tail;
    else
    {
        if(unlikely(!(last = queue->func_alloc_page(queue->privdata))))
            ERROR0(NULL, "queue->func_alloc_page(queue->privdata) failed");
        queue->page_count++;
        last->next = NULL;
        last->head = 0;
        last->tail = 0;
        queue->last->next = last;
        queue->last = last;
        entry = (char*)last + sizeof(struct queue_node);
    }
    last->tail++;
    queue->length++;
    return entry;
}

static void* generic_get_head(struct queue* queue, int remove)
{
    struct queue_node* first;
    void* entry;
    if(unlikely(!queue->length))
        return NULL;
    first = queue->first;
    assert(first);
    assert(first->tail <= queue->entry_per_page);
    if(unlikely(first->head == first->tail))
    {
        assert(first->tail == queue->entry_per_page);
        queue->first = first->next;
        assert(queue->first);
        queue->func_free_page(first, queue->privdata);
        assert(queue->page_count);
        queue->page_count--;
        first = queue->first;
    }
    assert(first->head < first->tail);
    entry = (char*)first + sizeof(struct queue_node) + queue->entry_size * first->head;
    if(remove)
    {
        first->head++;
        queue->length--;
    }
    return entry;
}

void* queue_take(struct queue* queue)
{
    assert(queue);
    return generic_get_head(queue, 1);
}

void* queue_glance(struct queue* queue)
{
    assert(queue);
    return generic_get_head(queue, 0);
}

void queue_deinit(struct queue* queue, void (*destructor)(void* value))
{
    struct queue_node* node;
    assert(queue);
    node = queue->first;
    while(node)
    {
        struct queue_node* next = node->next;
        assert(node->head <= node->tail);
        if(destructor)
        {
            size_t i = node->head;
            char* value = (char*)node + sizeof(struct queue_node) + queue->entry_size * i;
            for(; i < node->tail; i++)
            {
                destructor(value);
                value += queue->entry_size;
            }
        }
        queue->length -= node->tail - node->head;
        queue->func_free_page(node, queue->privdata);
        assert(queue->page_count);
        queue->page_count--;
        node = next;
    }
    assert(!queue->length);
    assert(!queue->page_count);
}

