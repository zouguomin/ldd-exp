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

#define N 10

/*对内核链表操作需要加锁*/
static struct mutex lock;

static struct list_head head;

struct my_data
{
	struct list_head list;
	int value;
};

/*链表的插入元素*/
struct list_head* insert_list(struct list_head *head,int value)
{
	struct my_data *md=NULL;
	mutex_lock(&lock);
	md=(struct my_data*)kmalloc(sizeof(struct my_data),GFP_KERNEL);
	if(md)
	{
		md->value=value;
		list_add(&md->list,head);
		
	}
	mutex_unlock(&lock);

	return head;
}
/*打印，传入参数v为open函数返回的，链表需要操作的节点*/
static int list_seq_show(struct seq_file *file,void *v)
{
	struct list_head *list=(struct list_head*)v;

	struct my_data *md=list_entry(list,struct my_data,list);

	seq_printf(file,"The value of my data is:%d\n",md->value);

	return 0;
}
static void *list_seq_start(struct seq_file *file,loff_t *pos)
{
	/*加锁*/
	mutex_lock(&lock);
	return seq_list_start(&head,*pos);
}

static void *list_seq_next(struct seq_file *file,void *v,loff_t *pos)
{
	return seq_list_next(v,&head,pos);
}
static void list_seq_stop(struct seq_file *file,void *v)
{
	/*解锁*/
	mutex_unlock(&lock);
}
static struct seq_operations list_seq_ops=
{
	.start	=list_seq_start,
	.next	=list_seq_next,
	.stop	=list_seq_stop,
	.show	=list_seq_show,
};

static int list_seq_open(struct inode *inode,struct file *file)
{
	return seq_open(file,&list_seq_ops);
}

static struct file_operations my_file_ops=
{
	.open	=list_seq_open,
	.read	=seq_read,
	.write	=seq_write,
	.llseek	=seq_lseek,
	.release=seq_release,
	.owner	=THIS_MODULE,
};

static __init int list_seq_init(void)
{
	struct proc_dir_entry *entry;
	int i;
	mutex_init(&lock);
	INIT_LIST_HEAD(&head);

	for(i=0;i<N;i++)
		head=*(insert_list(&head,i));

    #if 0
	entry=create_proc_entry("list_seq",0,NULL);
	if(entry)
		entry->proc_fops=&my_file_ops;
    #else
    entry = proc_create("list_seq", S_IRUGO | S_IWUSR, NULL, &my_file_ops);
    #endif
    
	return 0;
}

static void list_seq_exit(void)
{
	struct my_data *md=NULL;
	remove_proc_entry("list_seq",NULL);

	while(!list_empty(&head))
	{
		md=list_entry((&head)->next,struct my_data,list);
		list_del(&md->list);
		kfree(md);
	}
	
}

module_init(list_seq_init);
module_exit(list_seq_exit);


