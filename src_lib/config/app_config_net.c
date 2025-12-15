//
// Includes
//

// uni.hal
#include <uni_hal.h>

// ox
#include "net/ox_net_helpers.h"

// ox app
#include "config/app_config_net.h"

//
// Globals
//

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"



const uint32_t g_ox_net_icmp_addrs[OX_NET_ICMP_COUNT] = {
        IP_ADDR(172,17,33,15), //BBB A
        IP_ADDR(192,168,1,103), //BBB B
};

uint32_t g_ox_net_icmp_cnt_total[OX_NET_ICMP_COUNT] = {0};
uint32_t g_ox_net_icmp_cnt_ok[OX_NET_ICMP_COUNT] = {0};
uint32_t g_ox_net_icmp_cnt_timeout_seq[OX_NET_ICMP_COUNT] = {0};
uint32_t g_ox_net_icmp_time_last[OX_NET_ICMP_COUNT] = {0};


ox_net_context_t g_ox_net_ctx = {
        .config = {
                .ip_addr = IP_ADDR(172,17,33,15),
                .ip_mask = IP_ADDR(255, 255, 255, 0),
                .ip_gateway = IP_ADDR(0, 0, 0, 0),

                .dhcp_enable = false,
                .dns_addr = IP_ADDR(0, 0, 0, 0),

                .mac_addr =     {0xF2, 0x00, 0x00, 0x00, 0x00, 0x00},

                .icmp_enable = true,
                .icmp_timeout = 500,
                .icmp_interval = 500,
                .icmp_addr_count = OX_NET_ICMP_COUNT,
                .icmp_addr_vals = g_ox_net_icmp_addrs,
                .icmp_cnt_total = g_ox_net_icmp_cnt_total,
                .icmp_cnt_ok = g_ox_net_icmp_cnt_ok,
                .icmp_cnt_timeout_seq = g_ox_net_icmp_cnt_timeout_seq,
                .icmp_time_last = g_ox_net_icmp_time_last,
        }
};


//
// FTP
//

uni_net_ftp_client_context_t g_ox_ftp_ctx = {
    .config =  {
        .timeout_rx = 1000,
        .timeout_tx = 1000,
        .auth_user = "anonymous",
        .auth_password = "anonymous@anonymous.anonymous"
    }
};

void app_config_net_init(void) {
    uint32_t uid = uni_hal_core_stm32h7_uid_0();
    g_ox_net_ctx.config.mac_addr[3] = (uid >> 16) & 0xFF;
    g_ox_net_ctx.config.mac_addr[4] = (uid >> 8) & 0xFF;
    g_ox_net_ctx.config.mac_addr[5] = uid & 0xFF;
}
