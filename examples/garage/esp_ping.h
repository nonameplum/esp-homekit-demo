//
//  esp_ping.h
//  xcode
//
//  Created by Łukasz Śliwiński on 07/12/2019.
//  Copyright © 2019 plum. All rights reserved.
//

#ifndef esp_ping_h
#define esp_ping_h

#include "lwipopts.h"
#include <stdio.h>
#include <stdbool.h>
#include <lwip/ip_addr.h>
#include <lwip/icmp.h>
#include <strings.h>

#ifndef os_bzero
#define os_bzero(s, n) bzero(s, n)
#endif

#ifndef os_free
#define os_free(p) free((p))
#endif

#ifndef os_zalloc
#define os_zalloc(s) calloc(1, (s))
#endif

typedef int int32;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef signed char sint8_t;
typedef signed char sint8;
typedef unsigned char u8;

#define TRUE 1
#define FALSE 0

/**
 * PING_USE_SOCKETS: Set to 1 to use sockets, otherwise the raw api is used
 */
#ifndef PING_USE_SOCKETS
#define PING_USE_SOCKETS    LWIP_SOCKET
#endif

/**
 * PING_DEBUG: Enable debugging for PING.
 */
#ifndef PING_DEBUG
#define PING_DEBUG     LWIP_DBG_OFF
#endif

/** ping receive timeout - in milliseconds */
#ifndef PING_RCV_TIMEO
#define PING_RCV_TIMEO 1000
#endif

/** ping delay - in milliseconds */
#ifndef PING_COARSE
#define PING_COARSE     1000
#endif

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID        0xAFAF
#endif

/** ping additional data size to include in the packet */
#ifndef PING_DATA_SIZE
#define PING_DATA_SIZE 32
#endif

/** ping result action - no default action */
#ifndef PING_RESULT
#define PING_RESULT(ping_ok)
#endif

#define DEFAULT_PING_MAX_COUNT 4
#define PING_TIMEOUT_MS 1000

typedef void (* ping_recv_function)(void* arg, void *pdata);
typedef void (* ping_sent_function)(void* arg, void *pdata);

struct ping_option{
    uint32 count;
    uint32 ip;
    uint32 coarse_time;
    ping_recv_function recv_function;
    ping_sent_function sent_function;
    void* reverse;
};

struct ping_msg{
    struct ping_option *ping_opt;
    struct raw_pcb *ping_pcb;
    uint32 ping_start;
    uint32 ping_sent;
    uint32 timeout_count;
    uint32 max_count;
    uint32 sent_count;
    uint32 coarse_time;
};

struct ping_resp{
    uint32 total_count;
    uint32 resp_time;
    uint32 seqno;
    uint32 timeout_count;
    uint32 bytes;
    uint32 total_bytes;
    uint32 total_time;
    sint8  ping_err;
};

bool ping_start(struct ping_option *ping_opt);
bool ping_regist_recv(struct ping_option *ping_opt, ping_recv_function ping_recv);
bool ping_regist_sent(struct ping_option *ping_opt, ping_sent_function ping_sent);

#endif /* esp_ping_h */
