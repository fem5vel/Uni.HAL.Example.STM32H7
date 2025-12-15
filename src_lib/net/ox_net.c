//
// Includes
//

// FreeRTOS
#include <FreeRTOS_Routing.h>
#include "iperf_task.h"

// Uni.HAL
#include <uni_hal.h>

// os
#include "net/ox_net.h"


//
// Globals
//

static ox_net_context_t* g_ox_net_ctx_current = nullptr;

static uni_hal_gpio_pin_context_t pin_eth_refclk ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A,
        .gpio_pin = UNI_HAL_GPIO_PIN_1,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

static uni_hal_gpio_pin_context_t pin_eth_mdio ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A,
        .gpio_pin = UNI_HAL_GPIO_PIN_2,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

static uni_hal_gpio_pin_context_t pin_eth_crsdv ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_A,
        .gpio_pin = UNI_HAL_GPIO_PIN_7,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

static uni_hal_gpio_pin_context_t pin_eth_txd1 ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_B,
        .gpio_pin = UNI_HAL_GPIO_PIN_13,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

static uni_hal_gpio_pin_context_t pin_eth_mdc ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_C,
        .gpio_pin = UNI_HAL_GPIO_PIN_1,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

static uni_hal_gpio_pin_context_t pin_eth_rxd0 ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_C,
        .gpio_pin = UNI_HAL_GPIO_PIN_4,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

static uni_hal_gpio_pin_context_t pin_eth_rxd1 ={
        .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_C,
        .gpio_pin = UNI_HAL_GPIO_PIN_5,
        .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
        .gpio_speed = UNI_HAL_GPIO_SPEED_1,
        .alternate = UNI_HAL_GPIO_ALTERNATE_11
};

#if defined(TARGET_DEVICE_NUCLEO)
    static uni_hal_gpio_pin_context_t pin_eth_txen ={
            .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_G,
            .gpio_pin = UNI_HAL_GPIO_PIN_11,
            .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
            .gpio_speed = UNI_HAL_GPIO_SPEED_1,
            .alternate = UNI_HAL_GPIO_ALTERNATE_11
    };

    static uni_hal_gpio_pin_context_t pin_eth_txd0 ={
            .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_G,
            .gpio_pin = UNI_HAL_GPIO_PIN_13,
            .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
            .gpio_speed = UNI_HAL_GPIO_SPEED_1,
            .alternate = UNI_HAL_GPIO_ALTERNATE_11
    };
#else
#error "unknown device"
#endif


//
// Functions/ICMP
//

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier ) {
    switch (eStatus) {
        case eSuccess    :
            xQueueSend(g_ox_net_ctx_current->state.icmp_queue, &usIdentifier, 10 / portTICK_PERIOD_MS);
            break;
        case eInvalidChecksum :
        case eInvalidData :
        default:
            break;
    }
}


_Noreturn static void _ox_net_icmp_task( void * params) { //-V1082
    ox_net_context_t *ctx = (ox_net_context_t *) params;

    while (true) { //-V1044
        // check interface
        if (!ctx->state.net_endpoint.bits.bEndPointUp) {
            vTaskDelay(ctx->config.icmp_interval);
            continue;
        }

        if(!ctx->config.icmp_enable) {
            vTaskDelay(ctx->config.icmp_interval);
            continue;
        }

        for (uint32_t idx = 0; idx < ctx->config.icmp_addr_count; idx++) {
            xQueueReset(ctx->state.icmp_queue);

            // send request
            uint32_t net_addr = ctx->config.icmp_addr_vals[idx];
            uint32_t icmp_time_req = uni_hal_dwt_get_us();
            uint32_t icmp_time_resp = 0;
            uint32_t icmp_num_req = FreeRTOS_SendPingRequest(net_addr, 64, pdMS_TO_TICKS(ctx->config.icmp_timeout));
            uint32_t icmp_num_resp = 0;
            if (icmp_num_req != pdFAIL) {
                // wait response
                if (xQueueReceive(ctx->state.icmp_queue, &icmp_num_resp, ctx->config.icmp_timeout) == pdPASS) {
                    if (icmp_num_resp == icmp_num_req) {
                        icmp_time_resp = uni_hal_dwt_get_us();
                    }
                }
                xQueueReset(ctx->state.icmp_queue);
            }

            ctx->config.icmp_cnt_total[idx]++;
            if (icmp_num_resp != 0) {
                ctx->config.icmp_cnt_ok[idx]++;
                ctx->config.icmp_time_last[idx] = icmp_time_resp - icmp_time_req;
                ctx->config.icmp_cnt_timeout_seq[idx] = 0;
            } else {
                ctx->config.icmp_time_last[idx] = UINT32_MAX;
                ctx->config.icmp_cnt_timeout_seq[idx]++;
            }

            if (ctx->state.icmp_callback)
            {
                ctx->state.icmp_callback(ctx->state.icmp_callback_cookie, net_addr, ctx->config.icmp_time_last[idx]);
            }
        }

        // reboot on disconnect
        // bool reboot = true;
        // for (uint32_t idx = 0; idx < ctx->config.icmp_addr_count; idx++)
        // {
        //     if (ctx->config.icmp_cnt_timeout_seq[idx]  < 7)
        //     {
        //         reboot = false;
        //         break;
        //     }
        // }
        // if (reboot)
        // {
        //     uni_hal_pwr_reset();
        // }

        vTaskDelay(pdMS_TO_TICKS(ctx->config.icmp_interval));
    }
}



//
// Functions/Init
//

extern NetworkInterface_t * pxLinux_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t * pxInterface );
extern NetworkInterface_t * pxSTM32_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t * pxInterface );

bool ox_net_init(ox_net_context_t* ctx) {
    bool result = false;

    if (ctx != nullptr && ctx->state.initialized != true) {
        result = true;

        result = uni_hal_gpio_pin_init(&pin_eth_refclk) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_crsdv) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_mdc) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_mdio) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_rxd0) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_rxd1) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_txd0) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_txd1) && result;
        result = uni_hal_gpio_pin_init(&pin_eth_txen) && result;

        result = uni_hal_rcc_clk_set(UNI_HAL_CORE_PERIPH_ETH_1, true) && result;
        result = uni_hal_core_irq_enable(UNI_HAL_CORE_IRQ_ETH, 5, 0) && result;

#if defined(UNI_HAL_TARGET_MCU_PC)
        pxLinux_FillInterfaceDescriptor(0, &ctx->state.net_interface);
#else
        pxSTM32_FillInterfaceDescriptor(0, &ctx->state.net_interface);
#endif

        FreeRTOS_FillEndPoint(
                &ctx->state.net_interface,
                &ctx->state.net_endpoint,
                (const uint8_t *) &ctx->config.ip_addr,
                (const uint8_t *) &ctx->config.ip_mask,
                (const uint8_t *) &ctx->config.ip_gateway,
                (const uint8_t *) &ctx->config.dns_addr,
                ctx->config.mac_addr);
        if (ctx->config.dhcp_enable) {
            ctx->state.net_endpoint.bits.bWantDHCP = pdTRUE;
        }
        FreeRTOS_IPInit_Multi();

        //ICMP
        uni_hal_io_stdio_printf("ICMP init\r\n");
        ctx->state.icmp_queue = xQueueCreate(16U, sizeof(uint16_t));
        for (size_t idx = 0; idx < ctx->config.icmp_addr_count; idx++) {
            ctx->config.icmp_time_last[idx] = UINT32_MAX;
        }
        result = (xTaskCreate(_ox_net_icmp_task, "OX_NET_ICMP", configMINIMAL_STACK_SIZE * 4, ctx, 2,
                              &ctx->state.icmp_handle) == pdTRUE) && result;
        g_ox_net_ctx_current = ctx;


        ctx->state.initialized = true;
    }

    return result;
}


bool ox_net_is_inited(const ox_net_context_t* ctx) {
    return ctx != nullptr && ctx->state.initialized != false;
}


bool ox_net_is_online(const ox_net_context_t *ctx) {
    return ox_net_is_inited(ctx) && ctx->state.net_interface.bits.bInterfaceUp;
}

uint32_t ox_net_ping_get(const ox_net_context_t *ctx, uint32_t ip_addr) {
    uint32_t result = UINT32_MAX;
    if(ox_net_is_inited(ctx)) {
        for(size_t idx = 0; idx < ctx->config.icmp_addr_count; idx++) {
            if(ctx->config.icmp_addr_vals[idx] == ip_addr) {
                result = ctx->config.icmp_time_last[idx];
                break;
            }
        }
    }

    return result;
}

bool ox_net_ping_valid(const ox_net_context_t *ctx) {
    bool result = false;
    if (ox_net_is_inited(ctx)) {
        result = true;
        for (size_t idx = 0; idx < ctx->config.icmp_addr_count; idx++) {
            if (ctx->config.icmp_cnt_total[idx] == 0) {
                result = false;
                break;
            }
        }
    }

    return result;
}

bool ox_net_ping_register_callback(ox_net_context_t *ctx, ox_net_ping_callback_fn callback, void *callback_cookie)
{
    bool result = false;
    if (ctx != nullptr)
    {
        ctx->state.icmp_callback = callback;
        ctx->state.icmp_callback_cookie = callback_cookie;
        result = true;
    }

    return result;
}
