#pragma once

//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>

// Uni.HAL
#include <uni_hal.h>

// ox
#include "net/ox_net.h"


//
// Typedefs
//

typedef struct {
    uni_hal_io_context_t* io;
    ox_net_context_t* net;
    bool enable_timer;
} ox_stdio_config_t;


typedef struct {
    bool initialized;
    TaskHandle_t handle;
} ox_stdio_state_t;

typedef struct {
    ox_stdio_config_t config;
    ox_stdio_state_t state;
} ox_stdio_context_t;



//
// Functions
//

bool ox_stdio_init(ox_stdio_context_t* ctx);

bool ox_stdio_is_initialized(const ox_stdio_context_t* ctx);


