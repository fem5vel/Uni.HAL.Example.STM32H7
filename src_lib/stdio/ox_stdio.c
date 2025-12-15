//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stddef.h>

// unimcu
#include <uni_hal.h>

// ox
#include "stdio/ox_stdio.h"
#include "fwinfo/ox_fwinfo.h"


//
// Private
//

_Noreturn void _ox_stdio_task(void* params) {
    //-V1082
    ox_stdio_context_t *ctx = (ox_stdio_context_t *) params;

    uni_hal_io_stdio_printf("%c%c%c%c\r\n\r\n", 0x1B, 0x5B, 0x32, 0x4A); // clear screen


    const ox_fwinfo_t *fwinfo = ox_fwinfo_get_info(OX_FWINFO_CURRENT);
    uni_hal_io_stdio_printf("* Firmware Version = %s\r\n", fwinfo->git_version);
    uni_hal_io_stdio_printf("* Target Device    = %s\r\n", fwinfo->target_device);
    uni_hal_io_stdio_printf("* Build Timestamp  = %s\r\n", fwinfo->build_timestamp);
    uni_hal_io_stdio_printf("* Git Commit       = %s/%s\r\n", fwinfo->git_branch, fwinfo->git_commit_sha);

    char ipaddr[20] = {0};
    FreeRTOS_inet_ntop4(&ctx->config.net->config.ip_addr, ipaddr, sizeof(ipaddr));

    uni_hal_io_stdio_printf("* IP Address       = %s \r\n", ipaddr);
#if defined(STM32H7)
    uni_hal_io_stdio_printf("* MCU Revision     = %d \r\n", (int) uni_hal_core_stm32h7_revision_get());
#endif

    while (true) { //-V1044 //-V776
        uni_hal_io_stdio_printf("MEOW-MEOW\r\n");
        vTaskDelay(1000U);
    }

}



//
// Functions
//

bool ox_stdio_init(ox_stdio_context_t* ctx) {
    bool result = false;

    if(ctx != nullptr && ctx->state.initialized != true) {
        if (ctx->config.io != nullptr) {
            uni_hal_io_stdio_init(ctx->config.io);
        }

        xTaskCreate(_ox_stdio_task, "OX_STDIO", configMINIMAL_STACK_SIZE * 4, ctx, 1, &ctx->state.handle);

        ctx->state.initialized = true;
    }

    return result;
}


bool ox_stdio_is_initialized(const ox_stdio_context_t* ctx){
    return ctx != nullptr && ctx->state.initialized != false;
}
