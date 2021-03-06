/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 Google, Inc.
 *
 */
#ifndef __LINUX_TRUSTY_TRUSTY_IPC_H
#define __LINUX_TRUSTY_TRUSTY_IPC_H

#include <linux/device.h>

struct tipc_chan;

struct tipc_msg_buf {
	void *buf_va;
	phys_addr_t buf_pa;
	size_t buf_sz;
	size_t wpos;
	size_t rpos;
	struct list_head node;
	void *shm_handle;
};

enum tipc_chan_event {
	TIPC_CHANNEL_CONNECTED = 1,
	TIPC_CHANNEL_DISCONNECTED,
	TIPC_CHANNEL_SHUTDOWN,
};

struct tipc_chan_ops {
	void (*handle_event)(void *cb_arg, int event);
	struct tipc_msg_buf *(*handle_msg)(void *cb_arg,
					   struct tipc_msg_buf *mb);
};

struct tipc_dn_chan {
	int state;
	struct mutex lock; /* protects rx_msg_queue list and channel state */
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct completion reply_comp;
	struct list_head rx_msg_queue;
#if defined(CONFIG_MTK_GZ_KREE)
	u32 session;
#endif
};

struct tipc_chan *tipc_create_channel(struct device *dev,
				      const struct tipc_chan_ops *ops,
				      void *cb_arg);

int tipc_chan_connect(struct tipc_chan *chan, const char *port);

int tipc_chan_queue_msg(struct tipc_chan *chan, struct tipc_msg_buf *mb);

int tipc_chan_shutdown(struct tipc_chan *chan);

void tipc_chan_destroy(struct tipc_chan *chan);

struct tipc_msg_buf *tipc_chan_get_rxbuf(struct tipc_chan *chan);

void tipc_chan_put_rxbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb);

struct tipc_msg_buf *
tipc_chan_get_txbuf_timeout(struct tipc_chan *chan, long timeout);

void tipc_chan_put_txbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb);

static inline size_t mb_avail_space(struct tipc_msg_buf *mb)
{
	return mb->buf_sz - mb->wpos;
}

static inline size_t mb_avail_data(struct tipc_msg_buf *mb)
{
	return mb->wpos - mb->rpos;
}

static inline void *mb_put_data(struct tipc_msg_buf *mb, size_t len)
{
	void *pos = (u8 *)mb->buf_va + mb->wpos;

	WARN_ON(mb->wpos + len > mb->buf_sz);
	mb->wpos += len;

	return pos;
}

static inline void *mb_get_data(struct tipc_msg_buf *mb, size_t len)
{
	void *pos = (u8 *)mb->buf_va + mb->rpos;

	WARN_ON(mb->rpos + len > mb->wpos);
	mb->rpos += len;

	return pos;
}

#define tipc_k_handle void *

int tipc_k_connect(tipc_k_handle *h, const char *port);
int tipc_k_disconnect(tipc_k_handle h);

ssize_t tipc_k_read(tipc_k_handle h, void *buf, size_t buf_len,
						unsigned int flags);
ssize_t tipc_k_write(tipc_k_handle h, void *buf, size_t len,
						unsigned int flags);

#endif /* __LINUX_TRUSTY_TRUSTY_IPC_H */

