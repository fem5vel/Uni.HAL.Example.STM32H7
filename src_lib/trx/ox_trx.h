#pragma once

//
// Includes
//

// stdlib
#include <stdint.h>

// FreeRTOS
#include <FreeRTOS.h>
#include <task.h>

// Uni.Common
#include <uni_common.h>

// Uni.HAL
#include <uni_hal.h>

// ox
#include "trx/ox_trx_types.h"


/**
 * TRX Config
*/

typedef struct {
    uni_hal_spi_context_t* spi;
    uni_common_array_t* buf;
} ox_trx_config_t;

/**
 * TRX Context
 */
typedef struct {
    bool initialized;
    TaskHandle_t handle;
    uint32_t sent_total;

    QueueHandle_t queue_tosend;
    ox_trx_datamsg_t* in_transfer;
} ox_trx_state_t;

typedef struct {
    ox_trx_config_t config;
    ox_trx_state_t state;
} ox_trx_context_t;

//
// Functions
//

bool ox_trx_init(ox_trx_context_t* ctx);
bool ox_trx_is_initialized(const ox_trx_context_t* ctx);
ox_trx_datamsg_t* ox_trx_get_free_buffer(ox_trx_context_t* ctx);
bool ox_trx_send_buffer(ox_trx_context_t* ctx, ox_trx_datamsg_t* msg);
