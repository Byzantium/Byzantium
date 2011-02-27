/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */



/* Kernel Programming */
#define LINUX

#define SUCCESS 0
#define IOCGETNWDEV 1

#define DRIVER_AUTHOR "Marek Lindner <lindner_marek@yahoo.de>"
#define DRIVER_DESC   "B.A.T.M.A.N. performance accelerator"
#define DRIVER_DEVICE "batman"


#include <linux/module.h>    /* needed by all modules */
#include <linux/version.h>   /* LINUX_VERSION_CODE */
#include <linux/kernel.h>    /* KERN_ALERT */
#include <linux/fs.h>        /* struct inode */
#include <linux/socket.h>    /* sock_create(), sock_release() */
#include <linux/net.h>       /* SOCK_RAW */
#include <linux/in.h>        /* IPPROTO_RAW */
#include <linux/inet.h>      /* SOCK_RAW */
#include <linux/ip.h>        /* iphdr */
#include <linux/udp.h>       /* udphdr */
#include <linux/file.h>      /* get_unused_fd() */
#include <linux/fsnotify.h>  /* fsnotify_open() */
#include <linux/syscalls.h>  /* sys_close() */
#include <linux/sched.h>     /* schedule_timeout() */
#include <asm/uaccess.h>     /* get_user() */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	#include <linux/devfs_fs_kernel.h>
#else
#include "compat26.h"

	static struct class *batman_class;
#endif



static int device_open( struct inode *, struct file * );
static int device_release( struct inode *, struct file * );
static int device_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg );
static ssize_t device_write( struct file *, const char *, size_t, loff_t * );



struct orig_packet {
	struct iphdr ip;
	struct udphdr udp;
} __attribute__((packed));


struct minor {
	int minor_num;
	int in_use;
	struct class *minor_class;
	struct socket *raw_sock;
	struct kiocb kiocb;
	struct sock_iocb siocb;
	struct msghdr msg;
	struct iovec iov;
	struct sockaddr_in addr_out;
};


static struct file_operations fops = {
	.open = device_open,
	.release = device_release,
	.write = device_write,
	.ioctl = device_ioctl,
};



static int Major;                  /* Major number assigned to our device driver */
struct minor *minor_array[256];    /* minor numbers for device users */




int init_module( void ) {

	int i;

	/* register our device - kernel assigns a free major number */
	if ( ( Major = register_chrdev( 0, DRIVER_DEVICE, &fops ) ) < 0 ) {

		printk( "B.A.T.M.A.N.: Registering the character device failed with %d\n", Major );
		return Major;

	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	if ( devfs_mk_cdev( MKDEV( Major, 0 ), S_IFCHR | S_IRUGO | S_IWUGO, "batman", 0 ) )
		printk( "B.A.T.M.A.N.: Could not create /dev/batman \n" );
#else
	batman_class = class_create( THIS_MODULE, "batman" );

	if ( IS_ERR(batman_class) )
		printk( "B.A.T.M.A.N.: Could not register class 'batman' \n" );
	else
		device_create_drvdata( batman_class, NULL, MKDEV( Major, 0 ), NULL, "batman" );
#endif

	for ( i = 0; i < 255; i++ ) {
		minor_array[i] = NULL;
	}

	printk( "B.A.T.M.A.N.: I was assigned major number %d. To talk to\n", Major );
	printk( "B.A.T.M.A.N.: the driver, create a dev file with 'mknod /dev/batman c %d 0'.\n", Major );
	printk( "B.A.T.M.A.N.: Remove the device file and module when done.\n" );

	return SUCCESS;

}



void cleanup_module( void ) {

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	devfs_remove( "batman", 0 );
#else
	device_destroy_drvdata( batman_class, MKDEV( Major, 0 ) );
	class_destroy( batman_class );
#endif

	/* Unregister the device */
	int ret = unregister_chrdev( Major, DRIVER_DEVICE );

	if ( ret < 0 )
		printk( "B.A.T.M.A.N.: Unregistering the character device failed with %d\n", ret );

	printk( "B.A.T.M.A.N.: Unload complete\n" );

}



static int device_open( struct inode *inode, struct file *file ) {

	int minor_num, retval;
	struct minor *minor = NULL;


	if ( iminor( inode ) == 0 ) {

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
		MOD_INC_USE_COUNT;
#else
		try_module_get(THIS_MODULE);
#endif

	} else {

		minor_num = iminor( inode );
		minor = minor_array[minor_num];

		if ( minor == NULL ) {

			printk( "B.A.T.M.A.N.: open() only allowed on /dev/batman: minor number not registered \n" );
			return -EINVAL;

		}

		if ( minor->in_use > 0 ) {

			printk( "B.A.T.M.A.N.: open() only allowed on /dev/batman: minor number already in use \n" );
			return -EPERM;

		}

		if ( ( retval = sock_create_kern( PF_INET, SOCK_RAW, IPPROTO_RAW, &minor->raw_sock ) ) < 0 ) {

			printk( "B.A.T.M.A.N.: Can't create raw socket: %i", retval );
			return retval;

		}

		/* Enable broadcast */
		sock_valbool_flag( minor->raw_sock->sk, SOCK_BROADCAST, 1 );

		init_sync_kiocb( &minor->kiocb, NULL );

		minor->kiocb.private = &minor->siocb;

		minor->siocb.sock = minor->raw_sock;
		minor->siocb.scm = NULL;
		minor->siocb.msg = &minor->msg;

		minor->addr_out.sin_family = AF_INET;

		minor->msg.msg_iov = &minor->iov;
		minor->msg.msg_iovlen = 1;
		minor->msg.msg_flags = MSG_NOSIGNAL | MSG_DONTWAIT;
		minor->msg.msg_name = &minor->addr_out;
		minor->msg.msg_namelen = sizeof(minor->addr_out);
		minor->msg.msg_control = NULL;
		minor->msg.msg_controllen = 0;

		minor->in_use++;

	}

	return SUCCESS;

}



static int device_release( struct inode *inode, struct file *file ) {

	int minor_num;
	struct minor *minor = NULL;


	if ( ( minor_num = iminor( inode ) ) > 0 ) {

		minor = minor_array[minor_num];

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
		devfs_remove( "batman", minor_num );
#else
		device_destroy(minor->minor_class, MKDEV(Major, minor_num));
		class_destroy( minor->minor_class );
#endif
		sock_release( minor->raw_sock );

		kfree( minor );
		minor_array[minor_num] = NULL;

	}

	/* decrement usage count */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_DEC_USE_COUNT;
#else
	module_put(THIS_MODULE);
#endif

	return SUCCESS;

}



static ssize_t device_write( struct file *file, const char *buff, size_t len, loff_t *off ) {

	int minor_num;
	struct minor *minor = NULL;


	if ( len < sizeof(struct orig_packet) + 10 ) {

		printk( "B.A.T.M.A.N.: dropping data - packet too small (%i)\n", len );

		return -EINVAL;

	}

	if ( ( minor_num = iminor( file->f_dentry->d_inode ) ) == 0 ) {

		printk( "B.A.T.M.A.N.: write() not allowed on /dev/batman \n" );
		return -EPERM;

	}

	if ( !access_ok( VERIFY_READ, buff, sizeof(struct orig_packet) ) )
		return -EFAULT;

	minor = minor_array[minor_num];

	if ( minor == NULL ) {

		printk( "B.A.T.M.A.N.: write() - minor number not registered: %i \n", minor_num );
		return -EINVAL;

	}


	minor->iov.iov_base = buff;
	minor->iov.iov_len = len;

	__copy_from_user( &minor->addr_out.sin_port, &((struct orig_packet *)buff)->udp.dest, sizeof(minor->addr_out.sin_port) );
	__copy_from_user( &minor->addr_out.sin_addr.s_addr, &((struct orig_packet *)buff)->ip.daddr, sizeof(minor->addr_out.sin_addr.s_addr) );

	minor->siocb.size = len;

	return minor->raw_sock->ops->sendmsg( &minor->kiocb, minor->raw_sock, &minor->msg, len );

}



static int device_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg ) {

	int minor_num, fd;
	char filename[100];
	struct minor *minor = NULL;


	switch ( cmd ) {

		case IOCGETNWDEV:

			for ( minor_num = 1; minor_num < 255; minor_num++ ) {

				if ( minor_array[minor_num] == NULL ) {

					minor = kmalloc( sizeof(struct minor), GFP_KERNEL );

					if ( !minor )
						return -ENOMEM;

					memset( minor, 0, sizeof(struct minor) );
					minor_array[minor_num] = minor;

					break;

				}

			}

			if ( minor == NULL ) {

				printk( "B.A.T.M.A.N.: Maximum number of open batman instances reached \n" );
				return -EMFILE;

			}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
			if ( devfs_mk_cdev( MKDEV( Major, minor_num ), S_IFCHR | S_IRUGO | S_IWUGO, "batman%d", minor_num ) ) {
				printk( "B.A.T.M.A.N.: Could not create /dev/batman%d \n", minor_num );
#else
			sprintf( filename, "batman%d", minor_num );
			minor->minor_class = class_create( THIS_MODULE, filename );
			if ( IS_ERR(minor->minor_class) )
				printk( "B.A.T.M.A.N.: Could not register class '%s' \n", filename );
			else
				device_create_drvdata( minor->minor_class, NULL, MKDEV( Major, minor_num ), NULL, "batman%d", minor_num );
#endif
			/* let udev create the device file */
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(100);

			sprintf( filename, "/dev/batman%d", minor_num );

			fd = get_unused_fd();

			if ( fd >= 0 ) {

				struct file *f = filp_open( filename, O_WRONLY, 0660 );

				if ( IS_ERR(f) ) {

					put_unused_fd(fd);
					fd = PTR_ERR(f);

					printk( "B.A.T.M.A.N.: Could not open %s: %i \n", filename, fd );

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
					devfs_remove( "batman", minor_num );
#else
					device_destroy( minor->minor_class, MKDEV(Major, minor_num ));
					class_destroy( minor->minor_class );
#endif
					kfree( minor );
					minor_array[minor_num] = NULL;
					return fd;

				} else {

					fsnotify_open( f->f_dentry );
					fd_install( fd, f );

				}

			}

			/* increment usage count */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
			MOD_INC_USE_COUNT;
#else
			try_module_get(THIS_MODULE);
#endif

			return fd;
			break;

		default:

			if ( ( minor_num = iminor( inode ) ) == 0 ) {

				printk( "B.A.T.M.A.N.: ioctl( SO_BINDTODEVICE ) not allowed on /dev/batman \n" );
				return -EPERM;

			}

			minor = minor_array[minor_num];

			if ( minor == NULL ) {

				printk( "B.A.T.M.A.N.: ioctl( SO_BINDTODEVICE ) - minor number not registered: %i \n", minor_num );
				return -EINVAL;

			}

			return sock_setsockopt( minor->raw_sock, SOL_SOCKET, SO_BINDTODEVICE, (void __user *)arg, cmd );

	}

}





MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_SUPPORTED_DEVICE(DRIVER_DEVICE);

