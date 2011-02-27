/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Thomas Lopatic, Corinna 'Elektra' Aichele, Axel Neumann, Marek Lindner, Andreas Langer
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

#include "gateway24.h"
#include "hash.h"

static int batgat_open(struct inode *inode, struct file *filp);
static int batgat_release(struct inode *inode, struct file *file);
static int batgat_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg );

static int bat_netdev_setup( struct net_device *dev);
static int create_bat_netdev(void);
static int bat_netdev_open( struct net_device *dev );
static int bat_netdev_close( struct net_device *dev );
static int bat_netdev_xmit( struct sk_buff *skb, struct net_device *dev );

static struct gw_client *get_ip_addr(struct sockaddr_in *client_addr);
static int packet_recv_thread(void *data);
static void udp_data_ready(struct sock *sk, int len);

static int compare_wip( void *data1, void *data2 );
static int choose_wip( void *data, int32_t size );
static int compare_vip( void *data1, void *data2 );
static int choose_vip( void *data, int32_t size );

static int kernel_sendmsg(struct socket *sock, struct msghdr *msg, struct iovec *vec, size_t num, size_t size);
static int kernel_recvmsg(struct socket *sock, struct msghdr *msg, struct iovec *vec, size_t num, size_t size, int flags);
		
static struct file_operations fops = {
	.open = batgat_open,
	.release = batgat_release,
	.ioctl = batgat_ioctl,
};

static int Major;
static int thread_pid;
static struct completion thread_complete;
DECLARE_WAIT_QUEUE_HEAD(thread_wait);


spinlock_t hash_lock = SPIN_LOCK_UNLOCKED;

static struct net_device gate_device = {
	init: bat_netdev_setup, name: "gate%d",
};

atomic_t gate_device_run;
atomic_t data_ready_cond;

static struct hashtable_t *wip_hash;
static struct hashtable_t *vip_hash;
static struct list_head free_client_list;

int init_module(void)
{

	printk(KERN_DEBUG "B.A.T.M.A.N. gateway modul\n");
	
	if ( ( Major = register_chrdev( 0, DRIVER_DEVICE, &fops ) ) < 0 ) {

		DBG( "registering the character device failed with %d", Major );
		return Major;

	}

	DBG( "batgat loaded %s", strlen(REVISION_VERSION) > 3 ? REVISION_VERSION : "" );
	DBG( "I was assigned major number %d. To talk to", Major );
	DBG( "the driver, create a dev file with 'mknod /dev/batgat c %d 0'.", Major );
	printk(KERN_DEBUG "Remove the device file and module when done." );

	INIT_LIST_HEAD(&free_client_list);
	atomic_set(&gate_device_run, 0);

	/* TODO: error handling */
	vip_hash = hash_new( 128, compare_vip, choose_vip );
	wip_hash = hash_new( 128, compare_wip, choose_wip );
	
	printk(KERN_DEBUG "modul successfully loaded\n");
	return(0);
}

void cleanup_module(void)
{
	struct gate_priv *priv;
	struct free_client_data *entry, *next;
	struct gw_client *gw_client;
	
	unregister_chrdev( Major, DRIVER_DEVICE );

	if(thread_pid) {

		kill_proc(thread_pid, SIGTERM, 1 );
		wait_for_completion(&thread_complete);

	}

	if(atomic_read(&gate_device_run)) {

		priv = (struct gate_priv*)gate_device.priv;

		if( priv->tun_socket )
			sock_release(priv->tun_socket);

		kfree(gate_device.priv);
		gate_device.priv = NULL;
		unregister_netdev(&gate_device);
		atomic_dec(&gate_device_run);

	}
	
	list_for_each_entry_safe(entry, next, &free_client_list, list) {

		if(entry->gw_client != NULL) {
			gw_client = entry->gw_client;
			list_del(&entry->list);
			kfree(entry);
			kfree(gw_client);
		}

	}

	printk(KERN_DEBUG "modul successfully unloaded\n" );
	return;
}

static int batgat_open(struct inode *inode, struct file *filp)
{
	MOD_INC_USE_COUNT;
	return(0);
}

static int batgat_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return(0);
}


static int batgat_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg )
{
	uint8_t tmp_ip[4];
	struct batgat_ioc_args ioc;
	struct free_client_data *entry, *next;
	struct gw_client *gw_client;
	struct gate_priv *priv;
	
	int ret_value = 0;

	if( cmd == IOCSETDEV || cmd == IOCREMDEV ) {

		if( !access_ok( VERIFY_READ, (void *)arg, sizeof( ioc ) ) ) {
			printk(KERN_DEBUG "access to memory area of arg not allowed" );
			ret_value = -EFAULT;
			goto end;
		}

		if( __copy_from_user( &ioc, (void *)arg, sizeof( ioc ) ) ) {
			ret_value = -EFAULT;
			goto end;
		}

	}
	
	switch( cmd ) {

		case IOCSETDEV:

			if( ( ret_value = create_bat_netdev() ) == 0 && !thread_pid) {
				init_completion(&thread_complete);
				thread_pid = kernel_thread( packet_recv_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND );
				if(thread_pid<0) {
					printk(KERN_DEBUG "unable to start packet receive thread\n");
					ret_value = -EFAULT;
					goto end;
				}
			}

			if( ret_value == -1 || ret_value == 0 ) {

				tmp_ip[0] = 169; tmp_ip[1] = 254; tmp_ip[2] = 0; tmp_ip[3] = 0;
				ioc.universal = *(uint32_t*)tmp_ip;
				ioc.ifindex = gate_device.ifindex;

				strncpy( ioc.dev_name, gate_device.name, IFNAMSIZ - 1 );

				DBG("name %s index %d", ioc.dev_name, ioc.ifindex);
				
				if( ret_value == -1 ) {

					ioc.exists = 1;
					ret_value = 0;

				}

				if( copy_to_user( (void *)arg, &ioc, sizeof( ioc ) ) )

					ret_value = -EFAULT;

			}

			break;

		case IOCREMDEV:

			printk(KERN_DEBUG "disconnect daemon\n");

			if(thread_pid) {

				kill_proc(thread_pid, SIGTERM, 1 );
				wait_for_completion( &thread_complete );

			}

			thread_pid = 0;

			printk(KERN_DEBUG "thread shutdown\n");

// 			dev_put(gate_device);

			if(atomic_read(&gate_device_run)) {

				priv = (struct gate_priv*)gate_device.priv;

				if( priv->tun_socket )
					sock_release(priv->tun_socket);

				kfree(gate_device.priv);
				gate_device.priv = NULL;
				unregister_netdev(&gate_device);
				atomic_dec(&gate_device_run);
				printk(KERN_DEBUG "gate shutdown\n");

			}

			list_for_each_entry_safe(entry, next, &free_client_list, list) {

				gw_client = entry->gw_client;
				list_del(&entry->list);
				kfree(entry);
				kfree(gw_client);

			}

			printk(KERN_DEBUG "device unregistered successfully\n" );
			break;
		default:
			DBG( "ioctl %d is not supported",cmd );
			ret_value = -EFAULT;

	}

end:

		return( ret_value );
}

static int packet_recv_thread(void *data)
{
	sigset_t tmpsig;
	struct msghdr msg, inet_msg;
	struct iovec iov, inet_iov;
	struct iphdr *iph;
	struct gw_client *client_data;
	struct sockaddr_in client, inet_addr, server_addr;
	struct free_client_data *tmp_entry;

	struct socket *server_sock = NULL;
	struct socket *inet_sock = NULL;

	unsigned long time = jiffies;
	struct hash_it_t *hashit;

	int length, ret_value;
	unsigned char buffer[1600];

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = __constant_htons( (unsigned short) BATMAN_PORT );

	if ( ( sock_create( PF_INET, SOCK_RAW, IPPROTO_RAW, &inet_sock ) ) < 0 ) {

		printk(KERN_DEBUG "can't create raw socket\n");
		return -1;

	}

	if( sock_create( PF_INET, SOCK_DGRAM, IPPROTO_UDP, &server_sock ) < 0 ) {

		printk(KERN_DEBUG "can't create udp socket\n");
		sock_release(inet_sock);
		return -1;

	}


	if( server_sock->ops->bind(server_sock, (struct sockaddr *) &server_addr, sizeof( server_addr ) ) < 0 ) {

		printk(KERN_DEBUG "can't bind udp server socket\n");
		sock_release(server_sock);
		sock_release(inet_sock);
		return -1;

	}

	server_sock->sk->user_data = server_sock->sk->data_ready;
	server_sock->sk->data_ready = udp_data_ready;

	msg.msg_name = &client;
	msg.msg_namelen = sizeof( struct sockaddr_in );
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = MSG_NOSIGNAL | MSG_DONTWAIT;

	iph = ( struct iphdr*)(buffer + 1 );
	
	strcpy(current->comm,"udp_recv");
	daemonize();
	
	spin_lock_irq(&current->sigmask_lock);
	tmpsig = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) | sigmask(SIGTERM) );
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);


	atomic_set(&data_ready_cond, 0);

	while(!signal_pending(current)) {

		wait_event_interruptible(thread_wait, atomic_read(&data_ready_cond));

		if (signal_pending(current))
			break;

		client_data = NULL;

		iov.iov_base = buffer;
		iov.iov_len = sizeof(buffer);

		while ( ( length = kernel_recvmsg( server_sock, &msg, &iov, 1, sizeof(buffer), MSG_NOSIGNAL | MSG_DONTWAIT ) ) > 0 ) {

			if( ( jiffies - time ) / HZ > LEASE_TIME ) {

				hashit = NULL;
				spin_lock(&hash_lock);
				while( NULL != ( hashit = hash_iterate( wip_hash, hashit ) ) ) {
					client_data = hashit->bucket->data;
					if( ( jiffies - client_data->last_keep_alive ) / HZ > LEASE_TIME ) {

						hash_remove_bucket(wip_hash, hashit);
						hash_remove(vip_hash, client_data);

						tmp_entry = kmalloc(sizeof(struct free_client_data), GFP_KERNEL);
						if(tmp_entry != NULL) {
							tmp_entry->gw_client = client_data;
							list_add(&tmp_entry->list,&free_client_list);
						} else
							printk(KERN_DEBUG "can't add free gw_client to free list\n");

					}
				}
				spin_unlock(&hash_lock);
				time = jiffies;
			}

			if( length > 0 && buffer[0] == TUNNEL_IP_REQUEST ) {

				client_data = get_ip_addr(&client);

				if(client_data != NULL) {

					memcpy( &buffer[1], &client_data->vip_addr, sizeof( client_data->vip_addr ) );

					iov.iov_base = buffer;
					iov.iov_len = length;

					if( ( ret_value = kernel_sendmsg(server_sock, &msg, &iov, 1, length ) ) < 0 )
						DBG("tunnel ip request socket return %d", ret_value);

				} else
					printk(KERN_DEBUG "can't get an ip address\n");

			} else if( length > 0 && buffer[0] == TUNNEL_DATA ) {

				spin_lock(&hash_lock);
				client_data = ((struct gw_client *)hash_find(wip_hash, &client.sin_addr.s_addr));
				spin_unlock(&hash_lock);

				if(client_data == NULL) {

					buffer[0] = TUNNEL_IP_INVALID;
					iov.iov_base = buffer;
					iov.iov_len = length;

					if( ( ret_value = kernel_sendmsg(server_sock, &msg, &iov, 1, length ) ) < 0 )
						DBG("tunnel ip invalid socket return %d", ret_value);
					continue;

				}

				client_data->last_keep_alive = jiffies;

				inet_iov.iov_base = &buffer[1];
				inet_iov.iov_len = length - 1;

				inet_addr.sin_port = 0;
				inet_addr.sin_addr.s_addr = iph->daddr;

				if( (ret_value = kernel_sendmsg(inet_sock, &inet_msg, &inet_iov, 1, length - 1 ) ) < 0 )
					DBG("tunnel data socket return %d", ret_value);

			} else if( length > 0 && buffer[0] == TUNNEL_KEEPALIVE_REQUEST ) {

				spin_lock(&hash_lock);
				client_data = ((struct gw_client *)hash_find(wip_hash, &client.sin_addr.s_addr));
				spin_unlock(&hash_lock);
				if(client_data != NULL) {
					buffer[0] = TUNNEL_KEEPALIVE_REPLY;
					client_data->last_keep_alive = jiffies;
				} else
					buffer[0] = TUNNEL_IP_INVALID;

				iov.iov_base = buffer;
				iov.iov_len = length;

				if( ( ret_value = kernel_sendmsg(server_sock, &msg, &iov, 1, length ) ) < 0 )
					DBG("tunnel keep alive socket return %d", ret_value);

			} else
				printk(KERN_DEBUG "recive unknown message\n" );

		}

		iov.iov_base = buffer;
		iov.iov_len = sizeof(buffer);

		atomic_set(&data_ready_cond, 0);
	}
	
	if(server_sock)
		sock_release(server_sock);

	printk(KERN_DEBUG "thread terminated\n");
	complete(&thread_complete);
	return 0;
}

/* bat_netdev part */

static int bat_netdev_setup( struct net_device *dev )
{

	ether_setup(dev);
	dev->open = bat_netdev_open;
	dev->stop = bat_netdev_close;
	dev->hard_start_xmit = bat_netdev_xmit;
// 	dev->destructor = free_netdev;

	dev->features        |= NETIF_F_NO_CSUM;
	dev->hard_header_cache = NULL;
	dev->mtu = 1471;
	dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	dev->hard_header_cache = NULL;
	dev->priv = kmalloc(sizeof(struct gate_priv), GFP_KERNEL);

	if (dev->priv == NULL)
		return -ENOMEM;

	memset( dev->priv, 0, sizeof( struct gate_priv ) );
	
	return(0);
}

static int bat_netdev_xmit( struct sk_buff *skb, struct net_device *dev )
{
	struct gate_priv *priv = priv = (struct gate_priv*)dev->priv;
	struct sockaddr_in sa;
	struct iphdr *iph = ip_hdr( skb );
	struct iovec iov[2];
	struct msghdr msg;
	
	struct gw_client *client_data;
	unsigned char msg_number[1];

	msg_number[0] = TUNNEL_DATA;

	/* we use saddr , because hash choose and compare begin at + 4 bytes */
	spin_lock(&hash_lock);
	client_data = ((struct gw_client *)hash_find(vip_hash, & iph->saddr )); /* daddr */
	spin_unlock(&hash_lock);

	if( client_data != NULL ) {

		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = client_data->wip_addr;
		sa.sin_port = __constant_htons( (unsigned short)BATMAN_PORT );

		msg.msg_flags = MSG_NOSIGNAL | MSG_DONTWAIT;
		msg.msg_name = &sa;
		msg.msg_namelen = sizeof(sa);
		msg.msg_control = NULL;
		msg.msg_controllen = 0;

		iov[0].iov_base = msg_number;
		iov[0].iov_len = 1;
		iov[1].iov_base = skb->data + sizeof( struct ethhdr );
		iov[1].iov_len = skb->len - sizeof( struct ethhdr );
		kernel_sendmsg(priv->tun_socket, &msg, iov, 2, skb->len - sizeof( struct ethhdr ) + 1);

	} else
		printk(KERN_DEBUG "client not found\n");

	kfree_skb( skb );
	return( 0 );
}

static int bat_netdev_open( struct net_device *dev )
{
	printk(KERN_DEBUG "receive open\n" );
	netif_start_queue( dev );
	return( 0 );
}

static int bat_netdev_close( struct net_device *dev )
{

	printk(KERN_DEBUG "receive close\n" );
	netif_stop_queue( dev );
	return( 0 );
}

static int create_bat_netdev(void)
{

	struct gate_priv *priv;
	if(!atomic_read(&gate_device_run)) {

		if( ( register_netdev( &gate_device ) ) < 0 )
			return -ENODEV;

		priv = (struct gate_priv*)gate_device.priv;
		if( sock_create( PF_INET, SOCK_DGRAM, IPPROTO_UDP, &priv->tun_socket ) < 0 ) {

			printk(KERN_DEBUG "can't create gate socket\n");
			netif_stop_queue(&gate_device);
			return -EFAULT;

		}

		atomic_inc(&gate_device_run);

	} else {

		printk(KERN_DEBUG "net device already exists\n");
		return(-1);

	}

	return( 0 );
}

static struct gw_client *get_ip_addr(struct sockaddr_in *client_addr)
{
	static uint8_t next_free_ip[4] = {169,254,0,1};
	struct free_client_data *entry, *next;
	struct gw_client *gw_client = NULL;
	struct hashtable_t *swaphash;


	spin_lock(&hash_lock);
	gw_client = ((struct gw_client *)hash_find(wip_hash, &client_addr->sin_addr.s_addr));

	if (gw_client != NULL) {
		printk(KERN_DEBUG "found client in hash");
		spin_unlock(&hash_lock);
		return gw_client;
	}

	list_for_each_entry_safe(entry, next, &free_client_list, list) {
		printk(KERN_DEBUG "use free client from list");
		gw_client = entry->gw_client;
		list_del(&entry->list);
		kfree(entry);
		break;
	}

	if(gw_client == NULL) {
		printk(KERN_DEBUG "malloc client");
		gw_client = kmalloc( sizeof(struct gw_client), GFP_KERNEL );
		gw_client->vip_addr = 0;
	}

	gw_client->wip_addr = client_addr->sin_addr.s_addr;
	gw_client->client_port = client_addr->sin_port;
	gw_client->last_keep_alive = jiffies;

	/* TODO: check if enough space available */
	if (gw_client->vip_addr == 0) {

		gw_client->vip_addr = *(uint32_t *)next_free_ip;

		next_free_ip[3]++;

		if (next_free_ip[3] == 0)
			next_free_ip[2]++;

	}

	
	hash_add(wip_hash, gw_client);
	hash_add(vip_hash, gw_client);

	if (wip_hash->elements * 4 > wip_hash->size) {

		swaphash = hash_resize(wip_hash, wip_hash->size * 2);

		if (swaphash == NULL) {

			printk(KERN_DEBUG "Couldn't resize hash table" );

		}

		wip_hash = swaphash;

		swaphash = hash_resize(vip_hash, vip_hash->size * 2);

		if (swaphash == NULL) {

			printk(KERN_DEBUG "Couldn't resize hash table" );

		}

		vip_hash = swaphash;

	}
	spin_unlock(&hash_lock);
	return gw_client;

}

static void udp_data_ready(struct sock *sk, int len)
{

	void (*data_ready)(struct sock *, int) = sk->user_data;
	data_ready(sk,len);
	atomic_set(&data_ready_cond, 1);
	wake_up_interruptible(&thread_wait);

}

static int kernel_sendmsg(struct socket *sock, struct msghdr *msg, struct iovec *vec, size_t num, size_t size)
{
	mm_segment_t oldfs = get_fs();
	int result;

	set_fs(KERNEL_DS);

	msg->msg_iov = vec;
	msg->msg_iovlen = num;
	result = sock_sendmsg(sock, msg, size);
	set_fs(oldfs);
	return result;
}

static int kernel_recvmsg(struct socket *sock, struct msghdr *msg, struct iovec *vec, size_t num, size_t size, int flags)
{
	mm_segment_t oldfs = get_fs();
	int result;

	set_fs(KERNEL_DS);

	msg->msg_iov = (struct iovec *)vec, msg->msg_iovlen = num;
	result = sock_recvmsg(sock, msg, size, flags);
	set_fs(oldfs);
	return result;
}

int compare_wip(void *data1, void *data2)
{
	return ( !memcmp( data1, data2, 4 ) );
}

int compare_vip(void *data1, void *data2)
{
	return ( !memcmp( data1 + 4, data2 + 4, 4 ) );
}

int choose_wip(void *data, int32_t size)
{
	unsigned char *key= data;
	uint32_t hash = 0;
	size_t i;

	for (i = 0; i < 4; i++) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return (hash%size);

}

int choose_vip(void *data, int32_t size)
{
	unsigned char *key= data;
	uint32_t hash = 0;
	size_t i;
	for (i = 4; i < 8; i++) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return (hash%size);

}

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_SUPPORTED_DEVICE(DRIVER_DEVICE);
