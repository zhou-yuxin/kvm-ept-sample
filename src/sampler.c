#include "common.h"
#include "sampler.h"

#include <linux/fdtable.h>

#define INTERVAL_DELTA(hz_delta)    ((hz_delta) / 1000)
#define INIT_INTERVAL(hz)           (1000 * HZ / (hz))

static int get_kvm_by_vpid(pid_t nr, struct kvm** kvmp)
{
    struct pid* pid;
    struct task_struct* task;
    struct files_struct* files;
    int fd, max_fds;
    struct kvm* kvm = NULL;
    rcu_read_lock();
    if(!(pid = find_vpid(nr)))
    {
        rcu_read_unlock();
        ERROR1(-ESRCH, "no such process whose pid = %d", nr);
    }
    if(!(task = pid_task(pid, PIDTYPE_PID)))
    {
        rcu_read_unlock();
        ERROR1(-ESRCH, "no such process whose pid = %d", nr);
    }
    files = task->files;
    max_fds = files_fdtable(files)->max_fds;
    for(fd = 0; fd < max_fds; fd++)
    {
        struct file* file;
        char buffer[32];
        char* fname;
        if(!(file = fcheck_files(files, fd)))
            continue;
        fname = d_path(&(file->f_path), buffer, sizeof(buffer));
        if(fname < buffer || fname >= buffer + sizeof(buffer))
            continue;
        if(strcmp(fname, "anon_inode:kvm-vm") == 0)
        {
            kvm = file->private_data;
            assert(kvm);
            kvm_get_kvm(kvm);
            break;
        }
    }
    rcu_read_unlock();
    if(!kvm)
        ERROR1(-EINVAL, "process (pid = %d) has no kvm", nr);
    (*kvmp) = kvm;
    return 0;
}

static int get_ept_root(struct kvm* kvm, uint64_t** rootp)
{
    uint64_t root = 0;
    int i, vcpu_count = atomic_read(&(kvm->online_vcpus));
    for(i = 0; i < vcpu_count; i++)
    {
        struct kvm_vcpu* vcpu = kvm_get_vcpu(kvm, i);
        uint64_t root_of_vcpu;
        if(!vcpu)
            ERROR2(-ENXIO, "vcpu[%d] of process (pid = %d) is uncreated",
                i, kvm->userspace_pid);
        if(!(root_of_vcpu = vcpu->arch.mmu.root_hpa))
            ERROR1(-EINVAL, "vcpu[%d] is uninitialized", i);
        if(!root)
            root = root_of_vcpu;
        else if(root != root_of_vcpu)
            ERROR2(-EFAULT, "ept root of vcpu[%d] is %llx, different from other vcpus'",
                i, root_of_vcpu);
    }
    (*rootp) = root ? (uint64_t*)__va(root) : NULL;
    return 0;
}

static void update_interval(struct sampler* sampler)
{
    unsigned long current_time = jiffies, time_delta, hz, interval_delta;
    assert(current_time >= sampler->adapter.last_time);
    time_delta = current_time - sampler->adapter.last_time;
    if(time_delta < HZ)
        return;
    hz = HZ * sampler->adapter.triggers / time_delta;
    if(hz < sampler->hz)
    {
        interval_delta = MAX2(INTERVAL_DELTA(sampler->hz - hz), 1);
        if(sampler->adapter.interval <= interval_delta)
            sampler->adapter.interval = 1;
        else
            sampler->adapter.interval -= interval_delta;
    }
    else if(hz > sampler->hz)
    {
        interval_delta = MAX2(INTERVAL_DELTA(hz - sampler->hz), 1);
        sampler->adapter.interval += interval_delta;
    }
    sampler->adapter.last_time = current_time;
    sampler->adapter.triggers = 0;
}

#define EPT_PUD_ROOT(pgd)                                       \
({                                                              \
    uint64_t pgd_val = (pgd);                                   \
    uint64_t pud_root = pgd_val & (uint64_t)0xfffffffff000;     \
    pud_root ? (uint64_t*)__va(pud_root) : NULL;                \
})

#define EPT_PMD_ROOT(pud)                                       \
({                                                              \
    uint64_t pud_val = (pud);                                   \
    uint64_t pmd_root = 0;                                      \
    if(!(pud_val & 0x80))                                       \
        pmd_root = pud_val & (uint64_t)0xfffffffff000;          \
    pmd_root ? (uint64_t*)__va(pmd_root) : NULL;                \
})

#define EPT_WALK_TO_PMD(ept_root, action)                       \
({                                                              \
    uint64_t *pgds = (ept_root), *puds, *pmds, *pmdp;           \
    int i, j, k;                                                \
    for(i = 0; i < 512; i++)                                    \
    {                                                           \
        if(!(puds = EPT_PUD_ROOT(pgds[i])))                     \
            continue;                                           \
        for(j = 0; j < 512; j++)                                \
        {                                                       \
            if(!(pmds = EPT_PMD_ROOT(puds[j])))                 \
                continue;                                       \
            for(k = 0; k < 512; k++)                            \
            {                                                   \
                pmdp = pmds + k;                                \
                (action);                                       \
            }                                                   \
        }                                                       \
    }                                                           \
})

static void set_landmine_on_ept(struct timer_list* timer)
{
    struct sampler* sampler = container_of(timer, struct sampler, timer);
    uint64_t prot_mask = ~(sampler->prot_mask);
    EPT_WALK_TO_PMD(sampler->ept_root,
    ({
        uint64_t pmd_val = (*pmdp);
        if(pmd_val)
            __sync_bool_compare_and_swap(pmdp, pmd_val, pmd_val & prot_mask);
    }));
    update_interval(sampler);
    sampler->timer.expires += sampler->adapter.interval;
    add_timer(&(sampler->timer));
}

#define EPT_PGD_INDEX(addr)     (((addr) >> 39) & 0x1ff)
#define EPT_PUD_INDEX(addr)     (((addr) >> 30) & 0x1ff)
#define EPT_PMD_INDEX(addr)     (((addr) >> 21) & 0x1ff)

#define EPT_VIOLATION_ACC_READ      (1 << 0)
#define EPT_VIOLATION_ACC_WRITE     (1 << 1)
#define EPT_VIOLATION_ACC_INSTR     (1 << 2)
#define EPT_VIOLATION_ACC_ALL       (EPT_VIOLATION_ACC_READ | EPT_VIOLATION_ACC_WRITE | \
                                        EPT_VIOLATION_ACC_INSTR)

#define EPT_PROT_READ   (1 << 0)
#define EPT_PROT_WRITE  (1 << 1)
#define EPT_PROT_EXEC   (1 << 2)
#define EPT_PROT_UEXEC  (1 << 10)
#define EPT_PROT_ALL    (EPT_PROT_READ | EPT_PROT_WRITE | EPT_PROT_EXEC | EPT_PROT_UEXEC)

static int on_ept_sample(struct kvm* kvm, unsigned long gpa, unsigned long code)
{
    struct sampler* sampler = kvm->ept_sample_privdata;
    uint64_t *pgds, *puds, *pmds, *pmdp, pmd_val;
    if(!sampler)
        return 0;
    pgds = sampler->ept_root;
    if(!(puds = EPT_PUD_ROOT(pgds[EPT_PGD_INDEX(gpa)])))
        return 0;
    if(!(pmds = EPT_PMD_ROOT(puds[EPT_PUD_INDEX(gpa)])))
        return 0;
    pmdp = pmds + EPT_PMD_INDEX(gpa);
    pmd_val = (*pmdp);
    if(!pmd_val)
        return 0;
    if((pmd_val & EPT_PROT_ALL) == EPT_PROT_ALL)
        return 0;
    __sync_bool_compare_and_swap(pmdp, pmd_val, pmd_val | EPT_PROT_ALL);
    __sync_fetch_and_add(&(sampler->adapter.triggers), 1);
    sampler->func_on_sample(gpa, code & EPT_VIOLATION_ACC_ALL, sampler->privdata);
    return 1;
}

int sampler_init(struct sampler* sampler, pid_t pid,
    void (*func_on_sample)(unsigned long gpa, int xwr, void* privdata),
    void* privdata)
{
    int ret;
    struct kvm* kvm;
    assert(sampler);
    if(!(sampler->func_on_sample = func_on_sample))
        ERROR0(-EINVAL, "param <func_on_sample = NULL> is invalid");
    if((ret = get_kvm_by_vpid(pid, &kvm)))
        ERROR1(ret, "get_kvm_by_vpid(%d, &kvm) failed", pid);
    sampler->kvm = kvm;
    if((ret = get_ept_root(kvm, &(sampler->ept_root))))
    {
        kvm_put_kvm(kvm);
        ERROR0(ret, "get_ept_root(kvm, &(sampler->ept_root)) failed");
    }
    if(!__sync_bool_compare_and_swap(&(kvm->on_ept_sample), NULL, on_ept_sample))
    {
        kvm_put_kvm(kvm);
        ERROR1(-EIO, "kvm.on_ept_sample in process (pid = %d) has been occupied", pid);
    }
    wmb();
    sampler->prot_mask = EPT_PROT_ALL;
    sampler->hz = 0;
    init_timer_key(&(sampler->timer), set_landmine_on_ept, 0, NULL, NULL);
    sampler->privdata = privdata;
    wmb();
    assert(!kvm->ept_sample_privdata);
    kvm->ept_sample_privdata = sampler;
    return 0;
}

void sampler_set_prot(struct sampler* sampler, int xwr)
{
    assert(sampler);
    sampler->prot_mask = xwr & EPT_PROT_ALL;
}

void sampler_set_freq(struct sampler* sampler, unsigned long hz)
{
    assert(sampler);
    if(sampler->hz == 0 && hz != 0)
    {
        sampler->adapter.last_time = jiffies;
        sampler->adapter.triggers = 0;
        sampler->adapter.interval = INIT_INTERVAL(hz);
        sampler->timer.expires = jiffies;
        add_timer(&(sampler->timer));
    }
    else if(sampler->hz != 0 && hz == 0)
        del_timer_sync(&(sampler->timer));
    sampler->hz = hz;
}

void sampler_deinit(struct sampler* sampler)
{
    struct kvm* kvm;
    assert(sampler);
    kvm = sampler->kvm;
    assert(kvm);
    assert(kvm->on_ept_sample == on_ept_sample);
    assert(kvm->ept_sample_privdata == sampler);
    kvm->ept_sample_privdata = NULL;
    wmb();
    kvm->on_ept_sample = NULL;
    del_timer_sync(&(sampler->timer));
    EPT_WALK_TO_PMD(sampler->ept_root,
    ({
        uint64_t pmd_val = (*pmdp);
        if(pmd_val)
            __sync_bool_compare_and_swap(pmdp, pmd_val, pmd_val | EPT_PROT_ALL);
    }));
    kvm_put_kvm(kvm);
}
