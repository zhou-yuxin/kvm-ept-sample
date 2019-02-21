#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "hybridmem.h"
#include "kvm_ept_sample.h"

#define PAGE_SIZE           4096

#define MAX_MEMSLOTS        64
#define FORCE_REFRESH_LOOP  30

struct page_info
{
    uint64_t timestamp;         // the timestamp of latest update
    float temperature;          // the temperature of this page
    int node : 31;              // the NUMA node of this page
    unsigned node_inited : 1;   // is 'node' initiated
};

static uint64_t get_current_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static void update_page_info(struct page_info* page,
    uint64_t current_time, float half_life, float addition)
{
    assert(current_time >= page->timestamp);
    uint64_t time_delta = current_time - page->timestamp;
    page->temperature *= pow(0.5f, time_delta / half_life); // natural cooling
    page->temperature += addition;                          // temperation addition
    page->timestamp = current_time;                         // update timestamp
}

static struct hybridmem_page* build_hybridmem_input(struct page_info* pages,
    struct kvm_ept_sample_memslot* memslots, size_t memslot_count, int force_refresh_node,
    size_t* p_page_count)
{
    size_t max_page_count = 0;
    for(size_t i = 0; i < memslot_count; i++)
        max_page_count += memslots[i].page_count;
    struct hybridmem_page* input_pages = malloc(sizeof(struct hybridmem_page) * max_page_count);
    if(!input_pages)
        return NULL;
    size_t page_count = 0;
    for(size_t i = 0; i < memslot_count; i++)
    {
        uint64_t gfn = memslots[i].gpa / PAGE_SIZE;
        uint64_t hva = memslots[i].hva;
        size_t slot_page_count = memslots[i].page_count;
        for(size_t j = 0; j < slot_page_count; j++)
        {
            struct page_info* page = pages + gfn + j;
            if(force_refresh_node)
                page->node_inited = 0;
            int node;
            if(!page->node_inited)
                node = -1;
            else if(page->node >= 0)
                node = page->node;
            else
                continue;
            struct hybridmem_page* input_page = input_pages + page_count;
            input_page->address = hva + j * PAGE_SIZE;
            input_page->temperature = page->temperature;
            input_page->node = node;
            input_page->privdata = page;
            page_count++;
        }
    }
    assert(page_count <= max_page_count);
    (*p_page_count) = page_count;
    return input_pages;
}

static void on_page_migrated(unsigned long address, int status, void* privdata)
{
    struct page_info* page = privdata;
    assert(page);
    page->node = status;
    page->node_inited = 1;
}

int main(int argc, char** argv)
{
    pid_t pid;
    int xwr;
    unsigned long freq;
    float half_life, xaddition, waddition, raddition;
    unsigned long migration_delay, migration_interval, max_migration_bandwidth;
    int fast_node, slow_node;
    float fast_ratio, temperature_tolerance_ratio;
    if(argc != 15 ||
        // pid of target QEMU-KVM instance
        sscanf(argv[1], "%d", &pid) != 1 ||
        // access type to sample
        sscanf(argv[2], "%d", &xwr) != 1 ||
        // sample frequency
        sscanf(argv[3], "%lu", &freq) != 1 ||
        // half-life of natural cooling
        sscanf(argv[4], "%f", &half_life) != 1 ||
        // temperature addition when 'exec'
        sscanf(argv[5], "%f", &xaddition) != 1 ||
        // temperature addition when 'write'
        sscanf(argv[6], "%f", &waddition) != 1 ||
        // temperature addition when 'read'
        sscanf(argv[7], "%f", &raddition) != 1 ||
        // seconds before migration starts
        sscanf(argv[8], "%lu", &migration_delay) != 1 ||
        // seconds between loops of migration
        sscanf(argv[9], "%lu", &migration_interval) != 1 ||
        // max bandwitth to migrate
        sscanf(argv[10], "%lu", &max_migration_bandwidth) != 1 ||
        // NUMA node of fast memory device
        sscanf(argv[11], "%d", &fast_node) != 1 ||
        // NUMA node of slow memory device
        sscanf(argv[12], "%d", &slow_node) != 1 ||
        // the ratio of fast memory
        sscanf(argv[13], "%f", &fast_ratio) != 1 ||
        // the temperature anti-shaking tolerance ratio
        sscanf(argv[14], "%f", &temperature_tolerance_ratio) != 1)
    {
        fprintf(stderr, "USAGE: %s <pid> <xwr> <freq (Hz)> <half-life (ms)> <x-addition> "
            "<w-addition> <r-addition> <migration-delay (s)> <migration-interval (s)> "
            "<max-migration-bandwidth (MB/s)> <fast-node> <slow-node> <fast-ratio> "
            "<temperature-tolerance-ratio>\n", argv[0]);
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
    // set prot
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_PROT, xwr) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    // set frequency
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_SET_FREQ, freq) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    // get all memory slots
    struct kvm_ept_sample_memslot memslots[MAX_MEMSLOTS];
    struct kvm_ept_sample_get_memslots get_memslots =
    {
        .memslots = memslots,
        .capacity = MAX_MEMSLOTS,
        .count = 0,
    };
    if(ioctl(fd, KVM_EPT_SAMPLE_CMD_GET_MEMSLOTS, &get_memslots) < 0)
    {
        perror("ioctl() failed");
        return 1;
    }
    // ensure all memory slots are got
    assert(get_memslots.count < MAX_MEMSLOTS);
    uint64_t gpa_limit = 0, hva_limit = 0;
    for(size_t i = 0; i < get_memslots.count; i++)
    {
        size_t slot_size = memslots[i].page_count * PAGE_SIZE;
        uint64_t gpa_end = memslots[i].gpa + slot_size;
        uint64_t hva_end = memslots[i].hva + slot_size;
        if(gpa_end > gpa_limit)
            gpa_limit = gpa_end;
        if(hva_end > hva_limit)
            hva_limit = hva_end;
    }
    // allocate a linear array for all page information
    struct page_info* pages = mmap(NULL, sizeof(struct page_info) * gpa_limit / PAGE_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if((void*)pages == MAP_FAILED)
    {
        perror("mmap() failed");
        return 1;
    }
    // init libhybridmem
    struct hybridmem hybridmem;
    if(hybridmem_init(&hybridmem, pid, hva_limit / PAGE_SIZE, on_page_migrated))
    {
        fprintf(stderr, "hybridmem_init() failed\n");
        return 1;
    }
    uint64_t current_time = get_current_ms();
    // first time to scan migration tasks
    uint64_t migration_scan_time = current_time + migration_delay * 1000;
    // first time to execute migration tasks
    uint64_t migration_exec_time = migration_scan_time;
    size_t migration_scan_loops = 0;
    // endlessly read samples
    while(1)
    {
        current_time = get_current_ms();
        // if it's time to do a loop of migration scan
        if(current_time >= migration_scan_time)
        {
            struct hybridmem_page* input;
            size_t page_count;
            // build input data for libhybridmem
            if(!(input = build_hybridmem_input(pages, memslots, get_memslots.count,
                migration_scan_loops % FORCE_REFRESH_LOOP == 0, &page_count)))
            {
                fprintf(stderr, "build_hybridmem_input() failed\n");
                return 1;
            }
            size_t fast_page_count = (size_t)round(page_count * fast_ratio);
            // scan migration tasks
            ssize_t task_addition = hybridmem_scan(&hybridmem, input, page_count, fast_page_count,
                fast_node, slow_node, temperature_tolerance_ratio);
            if(task_addition < 0)
            {
                fprintf(stderr, "hybridmem_scan() failed\n");
                return 1;
            }
            // destroy the input data
            free(input);
            // set next migaration time
            migration_scan_time = current_time + migration_interval * 1000;
            migration_scan_loops++;
        }
        // if it's time to do a loop of migration execution
        if(current_time >= migration_exec_time)
        {
            // do 10% of max bandwidth in every 100ms
            size_t max_count = (size_t)round(max_migration_bandwidth * 256 / 10.0);
            size_t exec_count = hybridmem_execute(&hybridmem, max_count, 1);
            printf("\r%.3f MB/s                 ", 10.0 * exec_count / 256);
            fflush(stdout);
            // do it 100ms later
            migration_exec_time = current_time + 100;
        }
        struct kvm_ept_sample_sample samples[128];
        ssize_t len = read(fd, samples, sizeof(samples));
        if(len < 0)
        {
            perror("read() failed");
            return 1;
        }
        else if(len > 0)
        {
            assert(len % sizeof(struct kvm_ept_sample_sample) == 0);
            // count of samples
            size_t count = len / sizeof(struct kvm_ept_sample_sample);
            for(size_t i = 0; i < count; i++)
            {
                uint32_t gfn = samples[i].gfn;
                uint32_t xwr = samples[i].xwr;
                assert(gfn < gpa_limit / PAGE_SIZE);
                // get the according page_info
                struct page_info* page = pages + gfn;
                // get the according temperature addition
                float addition;
                if(xwr & 4)
                    addition = xaddition;       // addition for 'exec'
                else if(xwr & 2)
                    addition = waddition;       // addition for 'write'
                else
                    addition = raddition;       // addition for 'read'
                update_page_info(page, current_time, half_life, addition);
            }
        }
        else
            usleep(10000);
    }
    return 0;
}
