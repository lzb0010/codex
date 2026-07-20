#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define CHREVBASE_NAME "chrevbase"
#define CHREVBASE_MAJOR 200
#define KBUF_SIZE 1024

static const char kerbal[] = "kerbal rad";
static char kbuf[KBUF_SIZE];

static int chrevbase_open(struct inode *inode, struct file *filp)
{
    pr_info("chrevbase: open\n");
    return 0;
}

static int chrevbase_release(struct inode *inode, struct file *filp)
{
    pr_info("chrevbase: release\n");
    return 0;
}

static ssize_t chrevbase_read(struct file *filp, char __user *buf,
                              size_t count, loff_t *off)
{
    size_t len = sizeof(kerbal) - 1;

    if (*off >= len)
        return 0;
    if (count > len - *off)
        count = len - *off;
    if (copy_to_user(buf, kerbal + *off, count))
        return -EFAULT;
    *off += count;
    return count;
}

static ssize_t chrevbase_write(struct file *filp, const char __user *buf,
                               size_t count, loff_t *off)
{
    if (*off >= KBUF_SIZE)
        return -ENOSPC;
    if (count > KBUF_SIZE - *off)
        count = KBUF_SIZE - *off;
    if (copy_from_user(kbuf + *off, buf, count))
        return -EFAULT;
    *off += count;
    pr_info("chrevbase: wrote %zu bytes\n", count);
    return count;
}

static const struct file_operations chrevbase_fops = {
    .owner = THIS_MODULE,
    .open = chrevbase_open,
    .release = chrevbase_release,
    .read = chrevbase_read,
    .write = chrevbase_write,
};

static int __init chrevbase_init(void)
{
    int ret = register_chrdev(CHREVBASE_MAJOR, CHREVBASE_NAME,
                              &chrevbase_fops);
    if (ret < 0) {
        pr_err("chrevbase: register failed: %d\n", ret);
        return ret;
    }
    pr_info("chrevbase: registered\n");
    return 0;
}

static void __exit chrevbase_exit(void)
{
    unregister_chrdev(CHREVBASE_MAJOR, CHREVBASE_NAME);
}

module_init(chrevbase_init);
module_exit(chrevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lzb");
