//----------------------------------------------------------------------
//
// myass.c
//
// Linux port of the NotMyASS driver. This is a tiny module that exposes
// /dev/myass and intentionally panics the kernel when asked to crash.
//
// Crash reason is emitted as the hex encoding of "myass": 0x6D79617373.
//
//----------------------------------------------------------------------
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/string.h>

#define MYASS_NAME            "myass"
#define MYASS_IOCTL_CRASH      _IO('M', 0x06)

static const char kCrashReasonHex[] = "0x6D79617373";

static void
MyassCrash(void)
{
    panic("myass: crash reason %s", kCrashReasonHex);
}

static long
MyassIoctl(
    struct file *filp,
    unsigned int cmd,
    unsigned long arg
    )
{
    if (cmd == MYASS_IOCTL_CRASH || (cmd & 0xFF00) == 0x0000) {

        MyassCrash();
    }

    return -EINVAL;
}

static ssize_t
MyassWrite(
    struct file *filp,
    const char __user *user_buf,
    size_t len,
    loff_t *ppos
    )
{
    char buf[16];

    if (!user_buf || len == 0) {
        return -EINVAL;
    }

    if (len > sizeof(buf)) {
        len = sizeof(buf);
    }

    if (copy_from_user(buf, user_buf, len)) {
        return -EFAULT;
    }

    if (len >= sizeof("crash") - 1 && !strncmp(buf, "crash", sizeof("crash") - 1)) {
        MyassCrash();
    }

    return -EINVAL;
}

static const struct file_operations myass_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = MyassIoctl,
    .write          = MyassWrite,
};

static struct miscdevice myass_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = MYASS_NAME,
    .fops = &myass_fops,
};

static int __init
myass_init(void)
{
    int ret;

    pr_info("myass: loading Linux module, crash reason %s\n", kCrashReasonHex);

    ret = misc_register(&myass_misc);
    if (ret) {
        pr_err("myass: failed to create /dev/%s (%d)\n", MYASS_NAME, ret);
        return ret;
    }

    return 0;
}

static void __exit
myass_exit(void)
{
    misc_deregister(&myass_misc);
}

module_init(myass_init);
module_exit(myass_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI / Sysinternals port");
MODULE_DESCRIPTION("Linux NotMyASS driver replacement");
