//
// Includes
//

// stdlib
#include <string.h>
#include <stddef.h>

// Uni.Hal
#include <uni_hal.h>

// ox
#include "trx/ox_trx.h"


//
// Globals
//

//
// Private
//

static bool _ox_trx_spi_callback(void* pv) {
    BaseType_t high_priority_woken = pdFALSE;
    ox_trx_context_t *ctx = (ox_trx_context_t *)pv;

    ctx->state.in_transfer = NULL;
    ctx->state.sent_total++;

    return high_priority_woken;
}

//
// Functions
//

_Noreturn void _ox_trx_thread(void *params) {
    ox_trx_context_t *ctx = (ox_trx_context_t *)params;
    ox_trx_datamsg_t *buf;

    uni_hal_spi_set_callback(ctx->config.spi, _ox_trx_spi_callback, ctx);

    uni_hal_io_stdio_printf("[TRX] Thread started. Waiting for buffers...\r\n");

    while (true) {
        if (xQueueReceive(ctx->state.queue_tosend, &buf, portMAX_DELAY) != pdPASS) {
            taskYIELD();
            continue;
        }

        int wait_timeout = 0;
        while ((uni_hal_spi_is_busy(ctx->config.spi) || ctx->state.in_transfer != NULL) && wait_timeout < 100) {
            taskYIELD();
            wait_timeout++;
        }

        if (wait_timeout >= 100) {
            uni_hal_io_stdio_printf("[TRX] WARNING: SPI busy timeout\r\n");
            continue;
        }

#if defined(UNI_HAL_TARGET_MCU_STM32H743)
        uni_hal_core_cm7_dcache_cleaninvalidate_addr(buf, sizeof(ox_trx_datamsg_t));
#endif

        ctx->state.in_transfer = buf;
        uni_hal_io_stdio_printf("[TRX] Sending buffer %p . Total sent: %lu\r\n", (void *)buf, ctx->state.sent_total);

        if (!uni_hal_spi_transceive_async(ctx->config.spi, NULL,
                                          (const uint8_t *)buf, OX_TRX_MSG_SIZE)) {
            uni_hal_io_stdio_printf("[TRX] ERROR: Failed to start SPI transfer\r\n");
            ctx->state.in_transfer = NULL;
        }
    }
}


//
// Functions
//

bool ox_trx_init(ox_trx_context_t *ctx) {
    if (ctx == NULL || ctx->state.initialized) {
        return false;
    }

    ctx->state.queue_tosend = xQueueCreate(uni_common_array_size(ctx->config.buf),
                                            sizeof(ox_trx_datamsg_t*));
    if (ctx->state.queue_tosend == NULL) {
        uni_hal_io_stdio_printf("[TRX] ERROR: Failed to create queue\r\n");
        return false;
    }

    ctx->state.sent_total = 0;
    ctx->state.in_transfer = NULL;

    if (xTaskCreate(_ox_trx_thread, "OX_TRX", configMINIMAL_STACK_SIZE * 4,
                    ctx, 2, &ctx->state.handle) != pdTRUE) {
        vQueueDelete(ctx->state.queue_tosend);
        uni_hal_io_stdio_printf("[TRX] ERROR: Failed to create task\r\n");
        return false;
    }

    ctx->state.initialized = true;
    uni_hal_io_stdio_printf("[TRX] Context initialized\r\n");
    return true;
}


bool ox_trx_is_initialized(const ox_trx_context_t* ctx) {
    return ctx != NULL && ctx->state.initialized;
}

ox_trx_datamsg_t* ox_trx_get_free_buffer(ox_trx_context_t* ctx) {
    if (!ox_trx_is_initialized(ctx)) {
        uni_hal_io_stdio_printf("[TRX] ERROR: TRX not initialized\r\n");
        return NULL;
    }

    static size_t idx = 0;
    ox_trx_datamsg_t *buf = (ox_trx_datamsg_t*)uni_common_array_get(ctx->config.buf, idx);

    if (buf == NULL) {
        uni_hal_io_stdio_printf("[TRX] ERROR: Failed to get buffer at index %u\r\n", (unsigned int)idx);
    } else {
        uni_hal_io_stdio_printf("[TRX] Allocated buffer %p at index %u\r\n", (void *)buf, (unsigned int)idx);
    }

    idx = (idx + 1) % uni_common_array_size(ctx->config.buf);
    return buf;
}

// Отправить буфер в очередь
bool ox_trx_send_buffer(ox_trx_context_t* ctx, ox_trx_datamsg_t* msg) {
    if (!ox_trx_is_initialized(ctx) || msg == NULL) {
        return false;
    }

    return xQueueSend(ctx->state.queue_tosend, &msg, 0) == pdTRUE;
}
