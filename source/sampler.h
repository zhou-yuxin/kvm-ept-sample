#ifndef SAMPLER_H
#define SAMPLER_H

#include <linux/timer.h>
#include <linux/kvm_host.h>

struct sampler
{
    struct kvm* kvm;
    uint64_t* ept_root;
    uint64_t prot_mask;
	unsigned long hz;
    struct timer_list timer;
    struct
    {
        unsigned long last_time;
        unsigned long triggers;
        unsigned long interval;
    }
    adapter;
    void (*func_on_sample)(unsigned long gpa, int xwr, void* privdata);
    void* privdata;
};

int sampler_init(struct sampler* sampler, pid_t pid,
    void (*func_on_sample)(unsigned long gpa, int xwr, void* privdata),
    void* privdata);

void sampler_set_prot(struct sampler* sampler, int xwr);

void sampler_set_freq(struct sampler* sampler, unsigned long hz);

void sampler_deinit(struct sampler* sampler);

#endif
