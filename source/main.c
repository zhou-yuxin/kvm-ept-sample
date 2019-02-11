#include "common.h"
#include "interact.h"

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#define MODULE_NAME     "kvm_ept_sample"
#define MODULE_PROT     0666

static struct file_operations fops =
{
    .owner = THIS_MODULE,
    .open = interact_open,
    .unlocked_ioctl = interact_ioctl,
    .read = interact_read,
    .release = interact_release,
};

static int init(void)
{
    if(!proc_create(MODULE_NAME, MODULE_PROT, NULL, &fops))
        ERROR2(-EIO, "proc_create('%s', %x, NULL, &fops) failed", MODULE_NAME, MODULE_PROT);
    return 0;
}

static void cleanup(void)
{
    remove_proc_entry(MODULE_NAME, NULL);
}

module_init(init);
module_exit(cleanup);

MODULE_LICENSE("Dual BSD/GPL");
