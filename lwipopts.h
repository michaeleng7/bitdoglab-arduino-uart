#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// --- CRITICAL CONFIGURATIONS FOR FREERTOS ---

// NO_SYS must be 0 to enable the use of FreeRTOS tasks, mutexes, and semaphores
// for LwIP internal synchronization and threading.
#ifndef NO_SYS
#define NO_SYS                      1
#endif

// Enables LwIP core locking using FreeRTOS primitives (essential for stability)
#ifndef LWIP_TCPIP_CORE_LOCKING
#define LWIP_TCPIP_CORE_LOCKING     0
#endif

// Set number of internal system timeouts needed for LwIP (increased for FreeRTOS/Pico)
#define MEMP_NUM_SYS_TIMEOUT        10 

// Enables LwIP OS abstraction layer (required when NO_SYS is 0)
#ifndef LWIP_NETCONN
#define LWIP_NETCONN                0
#endif

// Allows memory allocation using libc functions (malloc/free)
#ifndef MEM_LIBC_MALLOC
#define MEM_LIBC_MALLOC             0
#endif

#define LWIP_SOCKET                 0
#define LWIP_PROVIDE_ERRNO         1

// Evita que o LwIP redefina as estruturas de sockets que a toolchain (newlib) já definiu
#define LWIP_TIMEVAL_PRIVATE 0
// --- GENERAL MEMORY AND PERFORMANCE SETTINGS ---
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

// --- NETWORK PROTOCOLS ---
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_DHCP                   1 

// --- TCP SETTINGS ---
#define TCP_WND                     16384
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_TCP_KEEPALIVE          1

// --- NETIF AND UTILITIES ---
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0
#define LWIP_CHKSUM_ALGORITHM       3

// --- DEBUG AND STATS (Keep stats off for release build) ---
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

#ifndef NDEBUG
#define LWIP_DEBUG                  1
#define LWIP_STATS                  1
#define LWIP_STATS_DISPLAY          1
#endif

// --- TLS / MBEDTLS REMOVAL (To avoid fatal errors with mbedtls/ssl.h) ---
// This is done because you are using non-secure MQTT (port 1883)
#define LWIP_ALTCP                  0
#define LWIP_ALTCP_TLS              0
#define LWIP_ALTCP_TLS_MBEDTLS      0

#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define MQTT_DEBUG                  LWIP_DBG_OFF

#endif /* _LWIPOPTS_H */