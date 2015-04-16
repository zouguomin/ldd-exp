#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/uaccess.h>


#define MLCD_XRES       400 //pixels per horizontal line
#define MLCD_YRES       240 //pixels per vertical line
#define MLCD_BYTES_LINE MLCD_XRES / 8 //number of bytes in a line
#define MLCD_BUF_SIZE   MLCD_YRES * MLCD_BYTES_LINE


struct _lcd_screen 
{
    unsigned char *buf;
    int up_flag;
    struct work_struct	task;
};

struct _lcd_screen lcd_screen[100];

struct kmem_cache *lcd_cachep = NULL;

//static struct workqueue_struct *queue=NULL;


int work_cnt = 0;
static void lcd_dis_task(struct work_struct *work)
{
	struct _lcd_screen *lcd =
		container_of(work, struct _lcd_screen, task);

	printk("&&&& %d \n", ++work_cnt);
	
	memset(lcd->buf, 0xff, MLCD_BUF_SIZE);
	
	kmem_cache_free(lcd_cachep, (void *)lcd->buf);
	
	kfree(lcd);
}

void lcd_task_run(int cnt)
{
	struct _lcd_screen *lcd = NULL;
	printk("work %d ", cnt); 
	
	lcd = (struct _lcd_screen *)kmalloc(sizeof(struct _lcd_screen), GFP_KERNEL);
	
	lcd->buf = (unsigned char *)kmem_cache_alloc(lcd_cachep, GFP_KERNEL);

	INIT_WORK(&lcd->task, lcd_dis_task);
	
	if(schedule_work(&lcd->task))
	{
		printk("success \n");
	}
	else
	{
		printk("fail \n");
	}

}

static int workqueue_test_init(void)
{
	int i; 
    printk("workqueue test init.\n");
	
	//queue=create_singlethread_workqueue("hello world");
	
	//INIT_WORK(&lcd_screen.task, lcd_dis_task);
	lcd_cachep = kmem_cache_create("lcd_cachep", MLCD_BUF_SIZE, 0,
			SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
	
	msleep(1000);
	
	for (i = 0; i < 100; i ++)
		lcd_task_run(i);
	
    return 0;
}

static void workqueue_test_exit(void)
{
    printk("workqueue test exit.\n");
}

module_init(workqueue_test_init);
module_exit(workqueue_test_exit);