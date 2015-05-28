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
#include <linux/list.h>

struct _lcd_screen 
{
    unsigned char *buf;
    int buf_cnt;
    struct list_head buf_list;
};

struct _lcd_screen lcd_screen;

typedef struct {
	int id;
	char *name;
	struct list_head _list;
}student;


void list_test_dump(void)
{
	struct list *node;
    student *stu;
    
    list_for_each_entry(stu, &lcd_screen.buf_list, _list) {
        
        printk("id:%d\n",stu->id);
    }
        
}

void list_test_add(int num)
{
    int i;
    student *stu;
    
	for (i = 0; i < num; i ++) {
		stu = kmalloc(sizeof(student), GFP_KERNEL);
		stu->id = i;
		
		list_add_tail(&stu->_list, &lcd_screen.buf_list);
	}
}

void list_test_destroy(void)
{
    student *stu;
    
	if (list_empty(&lcd_screen.buf_list))
    {
        printk("list empty \n");
		return;
    }

	stu = list_first_entry(&lcd_screen.buf_list, student, _list);    

    list_del(&stu->_list);
	printk("destroy id:%d\n", stu->id);
	kfree(stu);
}

static int list_test_init(void)
{
	int i; 
    printk("list test init.\n");
	
	INIT_LIST_HEAD(&lcd_screen.buf_list);
	
	// add list
	list_test_add(100);
	
	// dump list
	list_test_dump();

    for (i=0; i < 10; i ++)
        list_test_destroy();

    list_test_add(100);

    list_test_dump();

    return 0;
}

static void list_test_exit(void)
{
    printk("list test exit.\n");
	
	// destroy list
}

module_init(list_test_init);
module_exit(list_test_exit);
