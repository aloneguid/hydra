#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// This example uses a common include to avoid repetition
#include "lwipopts_examples_common.h"

// mDNS
#define LWIP_MDNS_RESPONDER 1
#define LWIP_IGMP 1
#define LWIP_NUM_NETIF_CLIENT_DATA 1
#define MDNS_RESP_USENETIF_EXTCALLBACK  1
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 3)

// Allow enough TCP PCBs for httpd connections + 1 WebSocket listen + 1 WebSocket client
#define MEMP_NUM_TCP_PCB 12

// Generated file containing html data
#define HTTPD_FSDATA_FILE "pico_fsdata.inc"

#endif
