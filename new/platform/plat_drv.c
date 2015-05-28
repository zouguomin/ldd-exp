#include <linux/module.h>  
#include <linux/init.h>  
#include <linux/platform_device.h>  
#include <linux/cdev.h>  
#include <linux/fs.h>  

#define CHRDEV_NAME "chrdev"  

/* device data  */
struct chrdev_platform_data {
    dev_t chrdev_devno;
    struct class *chrdev_class;  
    struct device *chrdev_device;    
    struct cdev chrdev_cdev;  
};

static struct chrdev_platform_data chrdev_data; 

static int chrdev_open(struct inode *inode, struct file *file) 
{  
    printk(KERN_ALERT "chrdev open!\n");  
    return 0;  
}  
  
static int chrdev_release(struct inode *inode, struct file *file) 
{  
    printk(KERN_ALERT "chrdev release!\n");  
    return 0;  
}  
  
static int chrdev_ioctl(struct inode *inode, struct file *file,  
    unsigned int cmd, unsigned long arg) 
{  
    printk(KERN_ALERT "chrdev ioctl!\n");  
    return 0;  
}  
 
static struct file_operations chrdev_fops = {  
    .owner      =   THIS_MODULE,  
    .ioctl      =   chrdev_ioctl,  
    .open       =   chrdev_open,  
    .release    =   chrdev_release,  
};  
    
static int chrdev_probe(struct platform_device *dev) 
{  
    int ret = 0, err = 0;  
      
    printk(KERN_ALERT "chrdev probe!\n");  

    struct chrdev_platform_data *pdata = dev->dev.platform_data;
      
    // alloc character device number  
    ret = alloc_chrdev_region(&(pdata->chrdev_devno), 0, 1, CHRDEV_NAME);  
    if (ret) {  
        printk(KERN_ALERT " alloc_chrdev_region failed!\n");  
        goto PROBE_ERR;  
    }  
    printk(KERN_ALERT " major:%d minor:%d\n", 
            MAJOR(pdata->chrdev_devno), MINOR(pdata->chrdev_devno));  
      
    cdev_init(&(pdata->chrdev_cdev), &chrdev_fops);  
    pdata->chrdev_cdev.owner = THIS_MODULE;  
    // add a character device  
    err = cdev_add(&(pdata->chrdev_cdev), pdata->chrdev_devno, 1);  
    if (err) {  
        printk(KERN_ALERT " cdev_add failed!\n");  
        goto PROBE_ERR;  
    }  
      
    // create the device class  
    pdata->chrdev_class = class_create(THIS_MODULE, CHRDEV_NAME);  
    if (IS_ERR(pdata->chrdev_class)) {  
        printk(KERN_ALERT " class_create failed!\n");  
        goto PROBE_ERR;  
    }  
      
    // create the device node in /dev  
    pdata->chrdev_device = device_create(pdata->chrdev_class, 
            NULL, pdata->chrdev_devno, NULL, CHRDEV_NAME);  
    if (NULL == pdata->chrdev_device) {  
        printk(KERN_ALERT " device_create failed!\n");  
        goto PROBE_ERR;  
    }  
      
    printk(KERN_ALERT " chrdev probe ok!\n");  
    return 0;  
      
PROBE_ERR:  
    if (err)  
        cdev_del(&(pdata->chrdev_cdev));  
    if (ret)   
        unregister_chrdev_region(pdata->chrdev_devno, 1);  
    return -1;  
}  
  
static int chrdev_remove (struct platform_device *dev) 
{  
    printk(KERN_ALERT " chrdev remove!\n");  

    struct chrdev_platform_data *pdata = dev->dev.platform_data;
    
    cdev_del(&(pdata->chrdev_cdev));  
    unregister_chrdev_region(pdata->chrdev_devno, 1);  
       
    device_destroy(pdata->chrdev_class, pdata->chrdev_devno);  
    class_destroy(pdata->chrdev_class);  
    return 0;  
}  
   
// platform_device and platform_driver must has a same name!  
// or it will not work normally  
static struct platform_driver chrdev_platform_driver = {  
    .probe  =   chrdev_probe,  
    .remove =   chrdev_remove,  
    .driver =   {  
        .name   =   CHRDEV_NAME,  
        .owner  =   THIS_MODULE,  
    },  
};  
  
static struct platform_device chrdev_platform_device = {  
    .name   =   CHRDEV_NAME,  
    .id     =   0,  
    //.dev    =   {  
    //}
    .dev.platform_data = &chrdev_data,
};  
    
static __init int chrdev_init(void) {  
    int ret = 0;  
    printk(KERN_ALERT "chrdev init!\n");  
      
    ret = platform_device_register(&chrdev_platform_device);  
    if (ret) {  
        printk(KERN_ALERT " platform_device_register failed!\n");  
        return ret;  
    }  
      
    ret = platform_driver_register(&chrdev_platform_driver);  
    if (ret) {  
        printk(KERN_ALERT " platform_driver_register failed!\n");  
        return ret;  
    }  
    
    printk(KERN_ALERT " chrdev_init ok!\n");  
    return ret;  
}  
  
static __exit void chrdev_exit(void) 
{  
    printk(KERN_ALERT "chrdev exit!\n");  
    platform_driver_unregister(&chrdev_platform_driver);  
}  
  
module_init(chrdev_init);  
module_exit(chrdev_exit);  

MODULE_LICENSE("Dual BSD/GPL");
