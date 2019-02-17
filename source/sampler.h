#ifndef SAMPLER_H
#define SAMPLER_H

#include <linux/timer.h>
#include <linux/kvm_host.h>

// A sampler to sample memory access on EPT
struct sampler
{
    struct kvm* kvm;            // the target KVM instance
    uint64_t* ept_root;         // the pointer to EPT
    uint64_t prot_mask;         // the mask to 'and' on EPT entry to set a landmine
	unsigned long hz;           // the desired frequency to sample
    struct timer_list timer;    // timmer to set landmines
    struct                      // a PID algorithm to adjuest the interval of 'timer'
    {
        unsigned long last_time;    // last timestamp
        unsigned long triggers;     // count of triggered landmines from 'last_time' to now
        unsigned long interval;     // the calculated interval for 'timer'
    }
    adapter;
    void (*func_on_sample)(unsigned long gpa, int xwr, void* privdata); // called upon a sample
    void* privdata;     // the private data passed to 'func_on_sample'
};

// init
//  pid: the pid of the QEMU process using KVM
//  function_on_sample: a function to be called back upon a sample
//      gpa: the Guest Physical Address
//      xwr: an 'or' bitmap of the access type. 'x' = execute, 'w' = write, 'r' = read
//          e.g. xwr = 100b means this access is to fetch instructions
//  privdata: the private data passed to 'func_on_sample'
// return 0 when ok, or a negative error code
int sampler_init(struct sampler* sampler, pid_t pid,
    void (*func_on_sample)(unsigned long gpa, int xwr, void* privdata),
    void* privdata);

// set the type of accesses to be sampled
//  xwr: an 'or' bitmap of the access type. 'x' = execute, 'w' = write, 'r' = read
//      e.g. xwr = 110b means both fetchin instructions and writing should be sampled
void sampler_set_prot(struct sampler* sampler, int xwr);

// set the frequency to sample
//  hz: the frequency
void sampler_set_freq(struct sampler* sampler, unsigned long hz);

// deinit
void sampler_deinit(struct sampler* sampler);

#endif
