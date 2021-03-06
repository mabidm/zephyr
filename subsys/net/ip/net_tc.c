/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_NET_DEBUG_TC)
#define SYS_LOG_DOMAIN "net/tc"
#define NET_LOG_ENABLED 1
#endif

#include <zephyr.h>
#include <string.h>

#include <net/net_core.h>
#include <net/net_pkt.h>
#include <net/net_stats.h>

#include "net_private.h"
#include "net_stats.h"

/* Stacks for TX work queue */
NET_STACK_ARRAY_DEFINE(TX, tx_stack,
		       CONFIG_NET_TX_STACK_SIZE,
		       CONFIG_NET_TX_STACK_SIZE,
		       NET_TC_TX_COUNT);

/* Stacks for RX work queue */
NET_STACK_ARRAY_DEFINE(RX, rx_stack,
		       CONFIG_NET_RX_STACK_SIZE,
		       CONFIG_NET_RX_STACK_SIZE + CONFIG_NET_RX_STACK_RPL,
		       NET_TC_RX_COUNT);

static struct net_traffic_class tx_classes[NET_TC_TX_COUNT];
static struct net_traffic_class rx_classes[NET_TC_RX_COUNT];

void net_tc_submit_to_tx_queue(u8_t tc, struct net_pkt *pkt)
{
	k_work_submit_to_queue(&tx_classes[tc].work_q, net_pkt_work(pkt));
}

void net_tc_submit_to_rx_queue(u8_t tc, struct net_pkt *pkt)
{
	k_work_submit_to_queue(&rx_classes[tc].work_q, net_pkt_work(pkt));
}

int net_tx_priority2tc(enum net_priority prio)
{
	/* FIXME: Initial implementation just maps the priority to certain
	 * traffic class to certain queue. This needs to be made more generic.
	 *
	 * Use the example priority -> traffic class mapper found in
	 * IEEE 802.1Q chapter 8.6.6 table 8.4 and chapter 34.5 table 34-1
	 *
	 *  Priority         Acronym   Traffic types
	 *  0 (lowest)       BK        Background
	 *  1 (default)      BE        Best effort
	 *  2                EE        Excellent effort
	 *  3 (highest)      CA        Critical applications
	 *  4                VI        Video, < 100 ms latency and jitter
	 *  5                VO        Voice, < 10 ms latency and jitter
	 *  6                IC        Internetwork control
	 *  7                NC        Network control
	 */
	/* Priority is the index to this array */
	static const u8_t tc[] = {
#if NET_TC_TX_COUNT == 1
		0, 0, 0, 0, 0, 0, 0, 0
#endif
#if NET_TC_TX_COUNT == 2
		0, 0, 1, 1, 0, 0, 0, 0
#endif
#if NET_TC_TX_COUNT == 3
		0, 0, 1, 2, 0, 0, 0, 0
#endif
#if NET_TC_TX_COUNT == 4
		0, 0, 2, 3, 1, 1, 1, 1
#endif
#if NET_TC_TX_COUNT == 5
		0, 0, 3, 4, 1, 1, 2, 2
#endif
#if NET_TC_TX_COUNT == 6
		0, 0, 4, 5, 1, 1, 2, 3
#endif
#if NET_TC_TX_COUNT == 7
		0, 0, 5, 6, 1, 2, 3, 4
#endif
#if NET_TC_TX_COUNT == 8
		0, 1, 6, 7, 2, 3, 4, 5
#endif
	};

	if (prio >= ARRAY_SIZE(tc)) {
		/* Use default value suggested in 802.1Q */
		prio = NET_PRIORITY_BE;
	}

	return tc[prio];
}

int net_rx_priority2tc(enum net_priority prio)
{
	/* FIXME: Initial implementation just maps the priority to certain
	 * traffic class to certain queue. This needs to be made more generic.
	 *
	 * Use the example priority -> traffic class mapper found in
	 * IEEE 802.1Q chapter 8.6.6 table 8.4 and chapter 34.5 table 34-1
	 *
	 *  Priority         Acronym   Traffic types
	 *  0 (lowest)       BK        Background
	 *  1 (default)      BE        Best effort
	 *  2                EE        Excellent effort
	 *  3 (highest)      CA        Critical applications
	 *  4                VI        Video, < 100 ms latency and jitter
	 *  5                VO        Voice, < 10 ms latency and jitter
	 *  6                IC        Internetwork control
	 *  7                NC        Network control
	 */
	/* Priority is the index to this array */
	static const u8_t tc[] = {
#if NET_TC_RX_COUNT == 1
		0, 0, 0, 0, 0, 0, 0, 0
#endif
#if NET_TC_RX_COUNT == 2
		0, 0, 1, 1, 0, 0, 0, 0
#endif
#if NET_TC_RX_COUNT == 3
		0, 0, 1, 2, 0, 0, 0, 0
#endif
#if NET_TC_RX_COUNT == 4
		0, 0, 2, 3, 1, 1, 1, 1
#endif
#if NET_TC_RX_COUNT == 5
		0, 0, 3, 4, 1, 1, 2, 2
#endif
#if NET_TC_RX_COUNT == 6
		0, 0, 4, 5, 1, 1, 2, 3
#endif
#if NET_TC_RX_COUNT == 7
		0, 0, 5, 6, 1, 2, 3, 4
#endif
#if NET_TC_RX_COUNT == 8
		0, 1, 6, 7, 2, 3, 4, 5
#endif
	};

	if (prio >= ARRAY_SIZE(tc)) {
		/* Use default value suggested in 802.1Q */
		prio = NET_PRIORITY_BE;
	}

	return tc[prio];
}

/* Convert traffic class to thread priority */
static u8_t tx_tc2thread(u8_t tc)
{
	/* Initial implementation just maps the traffic class to certain queue.
	 * If there are less queues than classes, then map them into
	 * some specific queue. In order to make this work same way as before,
	 * the thread priority 7 is used to map the default traffic class so
	 * this system works same way as before when TX thread default priority
	 * was 7.
	 *
	 * Lower value in this table means higher thread priority. The
	 * value is used as a parameter to K_PRIO_COOP() which converts it
	 * to actual thread priority.
	 *
	 * Higher traffic class value means higher priority queue. This means
	 * that thread_priorities[7] value should contain the highest priority
	 * for the TX queue handling thread.
	 */
	static const u8_t thread_priorities[] = {
#if NET_TC_TX_COUNT == 1
		7
#endif
#if NET_TC_TX_COUNT == 2
		8, 7
#endif
#if NET_TC_TX_COUNT == 3
		8, 7, 6
#endif
#if NET_TC_TX_COUNT == 4
		8, 7, 6, 5
#endif
#if NET_TC_TX_COUNT == 5
		8, 7, 6, 5, 4
#endif
#if NET_TC_TX_COUNT == 6
		8, 7, 6, 5, 4, 3
#endif
#if NET_TC_TX_COUNT == 7
		8, 7, 6, 5, 4, 3, 2
#endif
#if NET_TC_TX_COUNT == 8
		8, 7, 6, 5, 4, 3, 2, 1
#endif
	};

	BUILD_ASSERT_MSG(NET_TC_TX_COUNT <= CONFIG_NUM_COOP_PRIORITIES,
			 "Too many traffic classes");

	NET_ASSERT(tc < ARRAY_SIZE(thread_priorities));

	return thread_priorities[tc];
}

/* Convert traffic class to thread priority */
static u8_t rx_tc2thread(u8_t tc)
{
	/* Initial implementation just maps the traffic class to certain queue.
	 * If there are less queues than classes, then map them into
	 * some specific queue. In order to make this work same way as before,
	 * the thread priority 7 is used to map the default traffic class so
	 * this system works same way as before when RX thread default priority
	 * was 7.
	 *
	 * Lower value in this table means higher thread priority. The
	 * value is used as a parameter to K_PRIO_COOP() which converts it
	 * to actual thread priority.
	 *
	 * Higher traffic class value means higher priority queue. This means
	 * that thread_priorities[7] value should contain the highest priority
	 * for the RX queue handling thread.
	 */
	static const u8_t thread_priorities[] = {
#if NET_TC_RX_COUNT == 1
		7
#endif
#if NET_TC_RX_COUNT == 2
		8, 7
#endif
#if NET_TC_RX_COUNT == 3
		8, 7, 6
#endif
#if NET_TC_RX_COUNT == 4
		8, 7, 6, 5
#endif
#if NET_TC_RX_COUNT == 5
		8, 7, 6, 5, 4
#endif
#if NET_TC_RX_COUNT == 6
		8, 7, 6, 5, 4, 3
#endif
#if NET_TC_RX_COUNT == 7
		8, 7, 6, 5, 4, 3, 2
#endif
#if NET_TC_RX_COUNT == 8
		8, 7, 6, 5, 4, 3, 2, 1
#endif
	};

	BUILD_ASSERT_MSG(NET_TC_RX_COUNT <= CONFIG_NUM_COOP_PRIORITIES,
			 "Too many traffic classes");

	NET_ASSERT(tc < ARRAY_SIZE(thread_priorities));

	return thread_priorities[tc];
}

#if defined(CONFIG_NET_SHELL)
#define TX_STACK(idx) NET_STACK_GET_NAME(TX, tx_stack, 0)[idx].stack
#define RX_STACK(idx) NET_STACK_GET_NAME(RX, rx_stack, 0)[idx].stack
#else
#define TX_STACK(idx) NET_STACK_GET_NAME(TX, tx_stack, 0)[idx]
#define RX_STACK(idx) NET_STACK_GET_NAME(RX, rx_stack, 0)[idx]
#endif

#if defined(CONFIG_NET_STATISTICS)
/* Fixup the traffic class statistics so that "net stats" shell command will
 * print output correctly.
 */
static void tc_tx_stats_priority_setup(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		net_stats_update_tc_sent_priority(net_tx_priority2tc(i), i);
	}
}

static void tc_rx_stats_priority_setup(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		net_stats_update_tc_recv_priority(net_rx_priority2tc(i), i);
	}
}
#endif

/* Create workqueue for each traffic class we are using. All the network
 * traffic goes through these classes. There needs to be at least one traffic
 * class in the system.
 */
void net_tc_tx_init(void)
{
	int i;

	BUILD_ASSERT(NET_TC_TX_COUNT > 0);

#if defined(CONFIG_NET_STATISTICS)
	tc_tx_stats_priority_setup();
#endif

	for (i = 0; i < NET_TC_TX_COUNT; i++) {
		u8_t thread_priority;

		thread_priority = tx_tc2thread(i);
		tx_classes[i].tc = thread_priority;

#if defined(CONFIG_NET_SHELL)
		/* Fix the thread start address so that "net stacks"
		 * command will print correct stack information.
		 */
		NET_STACK_GET_NAME(TX, tx_stack, 0)[i].stack = tx_stack[i];
		NET_STACK_GET_NAME(TX, tx_stack, 0)[i].prio = thread_priority;
		NET_STACK_GET_NAME(TX, tx_stack, 0)[i].idx = i;
#endif

		NET_DBG("[%d] Starting TX queue %p stack %p size %zd "
			"prio %d (%d)", i,
			&tx_classes[i].work_q.queue, TX_STACK(i),
			K_THREAD_STACK_SIZEOF(tx_stack[i]),
			thread_priority, K_PRIO_COOP(thread_priority));

		k_work_q_start(&tx_classes[i].work_q,
			       tx_stack[i],
			       K_THREAD_STACK_SIZEOF(tx_stack[i]),
			       K_PRIO_COOP(thread_priority));
	}

	k_yield();
}

void net_tc_rx_init(void)
{
	int i;

	BUILD_ASSERT(NET_TC_RX_COUNT > 0);

#if defined(CONFIG_NET_STATISTICS)
	tc_rx_stats_priority_setup();
#endif

	for (i = 0; i < NET_TC_RX_COUNT; i++) {
		u8_t thread_priority;

		thread_priority = rx_tc2thread(i);
		rx_classes[i].tc = thread_priority;

#if defined(CONFIG_NET_SHELL)
		/* Fix the thread start address so that "net stacks"
		 * command will print correct stack information.
		 */
		NET_STACK_GET_NAME(RX, rx_stack, 0)[i].stack = rx_stack[i];
		NET_STACK_GET_NAME(RX, rx_stack, 0)[i].prio = thread_priority;
		NET_STACK_GET_NAME(RX, rx_stack, 0)[i].idx = i;
#endif

		NET_DBG("[%d] Starting RX queue %p stack %p size %zd "
			"prio %d (%d)", i,
			&rx_classes[i].work_q.queue, RX_STACK(i),
			K_THREAD_STACK_SIZEOF(rx_stack[i]),
			thread_priority, K_PRIO_COOP(thread_priority));

		k_work_q_start(&rx_classes[i].work_q,
			       rx_stack[i],
			       K_THREAD_STACK_SIZEOF(rx_stack[i]),
			       K_PRIO_COOP(thread_priority));
	}

	k_yield();
}
