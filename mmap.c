#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <asm/io.h>

//#define FAULT

/* methods of the character device */
static int mmap_open(struct inode *inode, struct file *filp);
static int mmap_release(struct inode *inode, struct file *filp);
static int mmap_mmap(struct file *filp, struct vm_area_struct *vma);

static void vm_open(struct vm_area_struct *vma);
static void vm_close(struct vm_area_struct *vma);
static vm_fault_t vm_fault(struct vm_fault *vmf);
static vm_fault_t vm_huge_fault(struct vm_fault *vmf, enum page_entry_size pe_size);

/* the file operations, i.e. all character device methods */
static struct file_operations mmap_fops = {
        .open = mmap_open,
        .release = mmap_release,
        .mmap = mmap_mmap,
        .owner = THIS_MODULE,
};

static struct miscdevice mmap_misc = {
        .minor = MISC_DYNAMIC_MINOR,
        .name = "mmap",
        .fops = &mmap_fops,
};

static struct vm_operations_struct vm_ops =
{
	.open = vm_open,
	.close = vm_close,
	.fault = vm_fault,
	.huge_fault = vm_huge_fault,
};

// internal data
// length of the two memory areas
#define NPAGES 16
// pointer to the vmalloc'd area - always page aligned
static u32 *vmalloc_area;
// pointer to the kmalloc'd area, rounded up to a page boundary
static int *kmalloc_area;
// original pointer for kmalloc'd area as returned by kmalloc
static void *kmalloc_ptr;

/* character device open method */
static int mmap_open(struct inode *inode, struct file *filp)
{
        pr_info("++%s\n", __func__);
        return 0;
}
/* character device last close method */
static int mmap_release(struct inode *inode, struct file *filp)
{
        pr_info("++%s\n", __func__);
        return 0;
}

static void vm_open(struct vm_area_struct *vma)
{
        pr_info("++%s\n", __func__);
}

static void vm_close(struct vm_area_struct *vma)
{
        pr_info("++%s\n", __func__);
}

static vm_fault_t vm_fault(struct vm_fault *vmf)
{
        struct vm_area_struct *vma = vmf->vma;
        struct page *page;

        pr_info("++%s\n", __func__);
        pr_info("pgoff: %d\n", vmf->pgoff);

        if (vmf == NULL) {
                pr_err("vms is NULL\n");
                return -1;
        }

        pr_info("vma->vm_pgoff: %d\n", vma->vm_pgoff);

        if (vmf->pgoff < NPAGES) {
                page = vmalloc_to_page((char *)vmalloc_area + PAGE_SIZE * vmf->pgoff);
        } else {
                page = virt_to_page((char *)kmalloc_area + PAGE_SIZE * (vmf->pgoff - NPAGES));
        }
        get_page(page);
        vmf->page = page;

        pr_info("--%s\n", __func__);
        return 0;
}

static vm_fault_t vm_huge_fault(struct vm_fault *vmf, enum page_entry_size pe_size)
{
        pr_info("++%s\n", __func__);
        return 0;
}

/* character device mmap method */
static int mmap_mmap(struct file *filp, struct vm_area_struct *vma)
{
        pr_info("++%s\n", __func__);

        vma->vm_ops = &vm_ops;
        vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
        vma->vm_private_data = filp->private_data;
        vm_open(vma);

        return 0;
}

/* module initialization - called at module load time */
static int __init mymmap_init(void)
{
        int ret = 0;
        int i;

        pr_info("++%s\n", __func__);

        /* allocate a memory area with kmalloc. Will be rounded up to a page boundary */
        if ((kmalloc_ptr = kmalloc((NPAGES + 2) * PAGE_SIZE, GFP_KERNEL)) == NULL) {
                ret = -ENOMEM;
                goto out;
        }
        /* round it up to the page bondary */
        kmalloc_area = (int *)((((unsigned long)kmalloc_ptr) + PAGE_SIZE - 1) & PAGE_MASK);

        pr_info("kmalloc_ptr: %p\n", kmalloc_ptr);
        pr_info("kmalloc_area: %p\n", kmalloc_area);

        /* allocate a memory area with vmalloc. */
        if ((vmalloc_area = (int *)vmalloc(NPAGES * PAGE_SIZE)) == NULL) {
                ret = -ENOMEM;
                goto out_kfree;
        }

        ret = misc_register(&mmap_misc);
        if (ret) {
                pr_err("can't misc_register\n");
                goto out_vfree;
        }

        /* mark the pages reserved */
        for (i = 0; i < NPAGES * PAGE_SIZE; i+= PAGE_SIZE) {
                SetPageReserved(vmalloc_to_page((void *)(((unsigned long)vmalloc_area) + i)));
                SetPageReserved(virt_to_page(((unsigned long)kmalloc_area) + i));
        }

        /* store a pattern in the memory - the test application will check for it */
        for (i = 0; i < (NPAGES * PAGE_SIZE / sizeof(int)); i++) {
                vmalloc_area[i] = i;
                kmalloc_area[i] = (0xdead << 16) + i;
                kmalloc_area[i + 1] = (0xbeef << 16) + i;
        }

#ifndef FAULT
        return ret;
#endif

  out_vfree:
        for (i = 0; i < NPAGES * PAGE_SIZE; i+= PAGE_SIZE) {
                ClearPageReserved(vmalloc_to_page((void *)(((unsigned long)vmalloc_area) + i)));
        }
        vfree(vmalloc_area);
  out_kfree:
        for (i = 0; i < NPAGES * PAGE_SIZE; i+= PAGE_SIZE) {
                ClearPageReserved(virt_to_page(((unsigned long)kmalloc_area) + i));
        }
        kfree(kmalloc_ptr);
  out:
        return ret;
}

/* module unload */
static void __exit mmap_exit(void)
{
        int i;

        pr_info("++%s\n", __func__);

        /* remove the character deivce */
        misc_deregister(&mmap_misc);

#ifndef FAULT
        /* unreserve the pages */
        for (i = 0; i < NPAGES * PAGE_SIZE; i+= PAGE_SIZE) {
                ClearPageReserved(vmalloc_to_page((void *)(((unsigned long)vmalloc_area) + i)));
                ClearPageReserved(virt_to_page(((unsigned long)kmalloc_area) + i));
        }
        /* free the memory areas */
        vfree(vmalloc_area);
        kfree(kmalloc_ptr);
#endif
}

module_init(mymmap_init);
module_exit(mmap_exit);
MODULE_DESCRIPTION("mmap demo driver");
MODULE_AUTHOR("Martin Frey <frey@scs.ch>");
MODULE_LICENSE("Dual BSD/GPL");
