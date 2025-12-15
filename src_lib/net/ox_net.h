#pragma once

//
// Includes
//

// FreeRTOS
#include <FreeRTOS_IP.h>


//
// Typedefs
//

typedef void (*ox_net_ping_callback_fn)(void *ctx_cookie, uint32_t ip_addr, uint32_t ping_time);

typedef struct {
    uint32_t          ip_addr;
    uint32_t          ip_mask;
    uint32_t          ip_gateway;

    bool              dhcp_enable;

    uint32_t          dns_addr;

    uint8_t           mac_addr[6];

    bool              icmp_enable;
    uint32_t          icmp_interval;
    uint32_t          icmp_timeout;
    uint32_t          icmp_addr_count;
    const uint32_t*   icmp_addr_vals;
    uint32_t*          icmp_cnt_total;
    uint32_t*          icmp_cnt_ok;
    uint32_t*          icmp_cnt_timeout_seq;
    uint32_t*          icmp_time_last;
} ox_net_config_t;


typedef struct {
    bool initialized;

    NetworkInterface_t net_interface;
    NetworkEndPoint_t net_endpoint;

    TaskHandle_t  icmp_handle;
    QueueHandle_t icmp_queue;

    ox_net_ping_callback_fn icmp_callback;
    void* icmp_callback_cookie;
} ox_net_state_t;


typedef struct {
    ox_net_config_t config;
    ox_net_state_t state;
} ox_net_context_t;



//
// Functions
//

bool ox_net_init(ox_net_context_t* ctx);

bool ox_net_is_inited(const ox_net_context_t* ctx);

bool ox_net_is_online(const ox_net_context_t* ctx);

uint32_t ox_net_ping_get(const ox_net_context_t* ctx, uint32_t ip_addr);

bool ox_net_ping_valid(const ox_net_context_t* ctx);

bool ox_net_ping_register_callback(ox_net_context_t *ctx, ox_net_ping_callback_fn callback, void *callback_ctx);
