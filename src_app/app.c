//
// Includes
//

// FreeRTOS Kernel
#include <FreeRTOS.h>
#include <task.h>

// unimcu
#include <uni_hal.h>
#include <io/uni_hal_io_stdio.h>

// ox
#include "config/app_config.h"
#include "config/app_config_crypto.h"
#include "config/app_config_net.h"
#include "config/app_config_uart.h"
#include "crypto/ox_hmac.h"
#include "net/ox_net.h"
#include "stdio/ox_stdio.h"
#include "segger/ox_segger_sysview.h"


//
// Globals
//

uint32_t __attribute__((section(".FREERTOS_HEAP"))) ucHeap[configTOTAL_HEAP_SIZE / sizeof(uint32_t)];



//
// Functions
//

int main(void) {
    // Cortex M7
    #if defined(UNI_HAL_TARGET_MCU_STM32H743)
        uni_hal_core_cm7_mpu_config();
        uni_hal_core_cm7_mpu_set(true);
        uni_hal_core_cm7_icache_set(true);
        uni_hal_core_cm7_dcache_set(true);
    #endif

    // PWR
    uni_hal_pwr_init();

    // IRQ
    uni_hal_core_irq_init();

    // RCC
#if !defined(UNI_HAL_TARGET_MCU_PC)
    uni_hal_rcc_stm32h7_config_set(&g_ox_rcc_config);
#endif
    uni_hal_rcc_init();

    // DWT
    uni_hal_dwt_init();

    // SEGGER
#if !defined(UNI_HAl_TARGET_MCU_PC)
    uni_hal_segger_rtt_stdio_init(&g_ox_segger_stdio_ctx);
#endif

    // UART
    uni_hal_usart_init(&g_ox_uart_ctx);

    // STDIO
    ox_stdio_init(&g_ox_stdio_ctx);

    // FLASH
    uni_hal_flash_init();

    uni_hal_io_stdio_printf("NET init\r\n");

    // NET
    app_config_net_init();
    ox_net_init(&g_ox_net_ctx);

    // Server HTTP
    app_config_server_init();

    // Crypto
    ox_hmac_init(&g_ox_hmac_ctx, g_ox_crypto_key, 32U);

    // SEGGER
#if !defined(UNI_HAL_TARGET_MCU_PC)
    //ox_segger_sysview_init();
#endif

    // Start FreeRTOS
    vTaskStartScheduler();

    // Unreachable
    return 0;
}

