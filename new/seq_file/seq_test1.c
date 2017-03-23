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

/*���ڲ���������*/
struct my_data
{
	int data;
};

/*ȫ�ֱ���*/
struct my_data *md;

/*���ݵ�����*/
struct my_data* my_data_init(void)
{
	int i;
	md=(struct my_data*)kmalloc(MAX_SIZE*sizeof(struct my_data),GFP_KERNEL);

	for(i=0;i<MAX_SIZE;i++)
		(md+i)->data=i;

	return md;
}

/*seq��start������������Խ���ж�Ȼ�󷵻�pos*/
void *my_seq_start(struct seq_file *file,loff_t *pos)
{
	return (*pos<MAX_SIZE)? pos :NULL;
}

/*seq��next������������Խ���ж�Ȼ��pos����*/
void *my_seq_next(struct seq_file *p,void *v,loff_t *pos)
{
	(*pos)++;
	if(*pos>=MAX_SIZE)
		return NULL;

	return pos;
}

/*seq��show�����������ݵ���ʾ*/
int my_seq_show(struct seq_file *file,void *v)
{
	unsigned int i=*(loff_t*)v;
	seq_printf(file,"The %d data is:%d\n",i,(md+i)->data);
	
	return 0;
}

/*seq��stop������ʲôҲ����*/
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

/*file��open����������seq�ļ��������ļ���ϵ*/
static int my_open(struct inode *inode,struct file *filp)
{
	return seq_open(filp,&my_seq_ops);
}

/*file����*/
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