//
// Includes
//

// ox app
#include "config/app_config_net.h"
#include "config/app_config_uart.h"
#include "config/app_config_segger.h"
#include "config/app_config_stdio.h"

//
// Globals
//


ox_stdio_context_t g_ox_stdio_ctx = {
        .config = {
                .io = &g_ox_uart_io_ctx,
                .net = &g_ox_net_ctx,
                .enable_timer = true,
        }
};
