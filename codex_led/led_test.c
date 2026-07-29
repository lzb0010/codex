/* led_test.c - i.MX6ULL LED Blink Test Driver */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define LED_GPIO  3

static struct timer_list blink_timer;
static int led_state = 0;

static void blink_callback(struct timer_list *t)
{
    led_state = !led_state;
    gpio_set_value(LED_GPIO, led_state);
    mod_timer(&blink_timer, jiffies + msecs_to_jiffies(500));
    printk(KERN_INFO "LED Test: GPIO%d = %d\n", LED_GPIO, led_state);
}

static int __init led_test_init(void)
{
    int ret;
    printk(KERN_INFO "LED Test: Initializing GPIO%d\n", LED_GPIO);
    ret = gpio_request(LED_GPIO, "led_test");
    if (ret) {
        printk(KERN_ERR "LED Test: Failed to request GPIO %d\n", LED_GPIO);
        return ret;
    }
    gpio_direction_output(LED_GPIO, 0);
    timer_setup(&blink_timer, blink_callback, 0);
    mod_timer(&blink_timer, jiffies + msecs_to_jiffies(500));
    printk(KERN_INFO "LED Test: Module loaded\n");
    return 0;
}

static void __exit led_test_exit(void)
{
    del_timer_sync(&blink_timer);
    gpio_set_value(LED_GPIO, 0);
    gpio_free(LED_GPIO);
    printk(KERN_INFO "LED Test: Module unloaded\n");
}

module_init(led_test_init);
module_exit(led_test_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AI Assistant");
MODULE_DESCRIPTION("LED Blink Test Driver for i.MX6ULL");
