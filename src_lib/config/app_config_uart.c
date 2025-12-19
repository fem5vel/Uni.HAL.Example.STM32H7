//
// Includes
//

// ox app
#include "config/app_config_uart.h"



//
// Globals/Pins
//

#if defined(TARGET_DEVICE_NUCLEO)
uni_hal_gpio_pin_context_t g_ox_uart_pin_rx_ctx = {
    .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_D,
    .gpio_pin = UNI_HAL_GPIO_PIN_8,
    .gpio_pull = UNI_HAL_GPIO_PULL_NO,
    .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
    .alternate = UNI_HAL_GPIO_ALTERNATE_7,
};

uni_hal_gpio_pin_context_t g_ox_uart_pin_tx_ctx = {
    .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_D,
    .gpio_pin = UNI_HAL_GPIO_PIN_9,
    .gpio_pull = UNI_HAL_GPIO_PULL_NO,
    .gpio_type = UNI_HAL_GPIO_TYPE_ALTERNATE_PP,
    .alternate = UNI_HAL_GPIO_ALTERNATE_7,
};
#else
#error "unknown device"
#endif



//
// Globals/IO
//

UNI_HAL_IO_DEFINITION(g_ox_uart_io, 512, 512);



//
// Globals/UART
//

uni_hal_usart_context_t g_ox_uart_ctx = {
    .instance = UNI_HAL_CORE_PERIPH_UART_3,
    .baudrate = 115200,
    .clksrc = UNI_HAL_RCC_CLKSRC_HSI,
    .io = &g_ox_uart_io_ctx,
    .isr_priority = 4,
    .initialized = false,
    .in_transmission = false,
    .pin_rx = &g_ox_uart_pin_rx_ctx,
    .pin_tx = &g_ox_uart_pin_tx_ctx,
};
