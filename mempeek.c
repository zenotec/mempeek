//*****************************************************************************
//    Copyright (C) 2014 ZenoTec LLC (http://www.zenotec.net)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//    File: rtap-ko.c
//    Description: Main module for rtap.ko. Contains top level module
//                 initialization and parameter parsing.
//
//*****************************************************************************

//*****************************************************************************
// Includes
//*****************************************************************************

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/highmem.h>

#include <asm/io.h>
#include <linux/ioport.h>

#include "mempeek.h"

/* Module information */
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

//*****************************************************************************
// Variables
//*****************************************************************************

/* Global */
extern struct resource iomem_resource;

/* Local */

static struct proc_dir_entry *mp_proc_dir = NULL;
static void *mp_addr = 0;
static uint32_t mp_len = 1;

//*****************************************************************************
// Functions
//*****************************************************************************

//*****************************************************************************
static int
mp_memcpy( uint32_t *buf_, uint32_t *addr_, size_t len_ )
{
    struct resource *res;
    struct page *p;
    void *virt_addr;

    /* Search for existing memory region resource */
    for( res = iomem_resource.child; res; res = res->sibling )
    {
        if( (res->start <= (uint32_t)addr_) && ((uint32_t)(addr_ + len_) <= res->end) )
        {
            printk( KERN_INFO "Resource: %s [0x%08X:0x%08X:0x%08lX]\n", res->name, res->start, res->end, res->flags );
            p = pfn_to_page((uint32_t)addr_ >> PAGE_SHIFT);
            virt_addr = kmap(p);
            buf_[0] = *(uint32_t *)virt_addr;
            kunmap(p);
        } // end if
    } // end for
    return( 0 );
}

//*****************************************************************************
static int
mp_proc_show( struct seq_file *file, void *arg )
{
    int i = 0;
    uint32_t *buf_p = 0;
    uint32_t *p = 0;

    /* Allocate buffer to read into */
    buf_p = kmalloc( (sizeof(uint32_t) * mp_len), GFP_KERNEL );
    if( !buf_p )
    {
        printk( KERN_ERR "Error allocating %d bytes\n", mp_len );
        return( -1 );
    } // end if

    /* Initialize buffer before reading from physical memory */
    memset( buf_p, 0xED, (sizeof(uint32_t) * mp_len) );

    /* Read memory */
    mp_memcpy( buf_p, mp_addr, mp_len );

    /* Print buffer */
    seq_printf( file, "Read %d bytes from address %p\n", mp_len, mp_addr );
    for( i = 0, p = (uint32_t *)mp_addr; i < mp_len; i++, p++ )
    {
        seq_printf( file, "%p: 0x%08X\n", p, buf_p[i] );
    } // end for

    kfree(buf_p);
    return( 0 );
}

//*****************************************************************************
static int
mp_proc_open( struct inode *inode, struct file *file )
{
    return( single_open( file, mp_proc_show, NULL ) );
}

//*****************************************************************************
static int
mp_proc_close( struct inode *inode, struct file *file )
{
    return( single_release( inode, file ) );
}

//*****************************************************************************
static ssize_t
mp_proc_read( struct file *file, char __user *buf, size_t cnt, loff_t *off )
{
    return( seq_read( file, buf, cnt, off ) );
}

//*****************************************************************************
static loff_t
mp_proc_lseek( struct file *file, loff_t off, int cnt )
{
    return( seq_lseek( file, off, cnt ) );
}

//*****************************************************************************
static ssize_t
mp_proc_write( struct file *file, const char __user *buf, size_t cnt, loff_t *off )
{
    int ret = 0;
    char addrstr[256+1] = { 0 };
    uint32_t l_addr = 0;
    uint32_t l_len = 1;

    cnt = (cnt >= 256) ? 256 : cnt;

    ret = copy_from_user( addrstr, buf, cnt );
    if( ret != 0 )
    {
        printk( KERN_ERR "Failed to copy string from user memory space: %d\n", ret );
        return( -1 );
    } // end if

    ret = sscanf( addrstr, "%x:%x", &l_addr, &l_len );
    if( ret < 1 || ret > 2 )
    {
        printk( KERN_ERR "Failed to parse address string: [%d] %s\n", ret, addrstr );
        printk( KERN_INFO "Address string '0xAAAAAAAA:0xLLLL'\n" );
        return( -1 );
    } // end else

    mp_addr = (void *)l_addr;
    mp_len = l_len;

    return( cnt );
}

//*****************************************************************************
//*****************************************************************************
const struct file_operations mp_proc_fops =
{
    .owner      = THIS_MODULE,
    .open       = mp_proc_open,
    .release    = mp_proc_close,
    .read       = mp_proc_read,
    .llseek     = mp_proc_lseek,
    .write      = mp_proc_write,
};


//*****************************************************************************
//*****************************************************************************
static int __init
mempeek_init( void )
{

    printk( KERN_INFO "Module '%s (%s)' registered.\n", DRIVER_NAME, DRIVER_VERSION );

    /* Initialize proc filesyster structure */
    mp_proc_dir = proc_mkdir( "mp", NULL );
    proc_create( "mp", 0660, mp_proc_dir, &mp_proc_fops );

    return( 0 );

}

//*****************************************************************************
static void __exit
mempeek_exit( void )
{

    remove_proc_entry( "mp", mp_proc_dir );
    remove_proc_entry( "mp", NULL );

    printk( KERN_INFO "Module unloaded\n" );

    return;

}

module_init(mempeek_init);
module_exit(mempeek_exit);
