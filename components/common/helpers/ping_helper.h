#ifndef LWIP_PING_H
#define LWIP_PING_H

#include "lwip/ip_addr.h"
#include <espressif/esp_wifi.h>

typedef void (*ping_watchdog_fail_fn)();

typedef enum {
    PING_RES_NO_MEM,                /* internal memory alloc failure */
    PING_RES_ERR_SENDING,           /* socket could not send */
    PING_RES_ERR_NO_SOCKET,         /* socket could not be created */
    PING_RES_TIMEOUT,               /* no response received in time */
    PING_RES_ID_OR_SEQNUM_MISMATCH, /* response ID or sequence number mismatched */
    PING_RES_ECHO_REPLY,            /* ping answer received */
    PING_RES_DESTINATION_UNREACHABLE, /* destination unreachable received */
    PING_RES_TIME_EXCEEDED,         /* for TTL to low or time during defrag exceeded (see wiki) */
    PING_RES_UNHANDLED_ICMP_CODE,   /* for all ICMP types which are not specifically handled */
} ping_result_code;

typedef struct {
    ping_result_code result_code;
    u32_t response_time_ms;
    ip_addr_t response_ip;
} ping_result_t;

void ping_ip(ip_addr_t ping_addr, ping_result_t *res);

ip_addr_t get_gw_ip();

void start_ping_watchdog(ip_addr_t ping_addr, int duration_sec, int max_failure_count, ping_watchdog_fail_fn on_fail_callback);

#endif /* LWIP_PING_H */
