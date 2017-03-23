#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

MODULE_AUTHOR("patrick");
MODULE_LICENSE("Dual BSD/GPL");

#define MAX_SIZE 10

/*用于操作的数据*/
struct my_data
{
	int data;
};

/*全局变量*/
struct my_data *md;

/*数据的申请*/
struct my_data* my_data_init(void)
{
	int i;
	md=(struct my_data*)kmalloc(MAX_SIZE*sizeof(struct my_data),GFP_KERNEL);

	for(i=0;i<MAX_SIZE;i++)
		(md+i)->data=i;

	return md;
}

/*seq的start函数，仅仅做越界判断然后返回pos*/
void *my_seq_start(struct seq_file *file,loff_t *pos)
{
	return (*pos<MAX_SIZE)? pos :NULL;
}

/*seq的next函数，仅仅做越界判断然后pos递增*/
void *my_seq_next(struct seq_file *p,void *v,loff_t *pos)
{
	(*pos)++;
	if(*pos>=MAX_SIZE)
		return NULL;

	return pos;
}

/*seq的show函数，读数据的显示*/
int my_seq_show(struct seq_file *file,void *v)
{
	unsigned int i=*(loff_t*)v;
	seq_printf(file,"The %d data is:%d\n",i,(md+i)->data);
	
	return 0;
}

/*seq的stop函数，什么也不做*/
void my_seq_stop(struct seq_file *file,void *v)
{

}

/*operations of seq_file */
static const struct seq_operations my_seq_ops={
	.start	=my_seq_start,
	.next	=my_seq_next,
	.stop	=my_seq_stop,
	.show	=my_seq_show,
};

/*file的open函数，用于seq文件与虚拟文件联系*/
static int my_open(struct inode *inode,struct file *filp)
{
	return seq_open(filp,&my_seq_ops);
}

/*file操作*/
static const struct file_operations my_file_ops={
	.open	=my_open,
	.read	=seq_read,
	.llseek	=seq_lseek,
	.release=seq_release,
	.owner	=THIS_MODULE,
};

static __init int my_seq_init(void)
{
	struct proc_dir_entry *p;
    
	my_data_init();
    #if 0
	p=create_proc_entry("my_seq",0,NULL);
	if(p)
	{
		p->proc_fops=&my_file_ops;
	}
    #else
	p = proc_create("my_seq", S_IRUGO | S_IWUSR, NULL,
		&my_file_ops);
	if (p == NULL) {
		printk("Unable to create /proc/my_seq entry");
		return 0;
	}
    #endif

	return 0;
	
}

static void my_seq_exit(void)
{
	remove_proc_entry("my_seq",NULL);
}

module_init(my_seq_init);
module_exit(my_seq_exit);