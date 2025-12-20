#include <uni_common.h>
#include "app_config_trx.h"

// GPIO/SPI
uni_hal_gpio_pin_context_t g_ox_trx_pin_spi_mosi = {
    .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_E,
    .gpio_pin = UNI_HAL_GPIO_PIN_14,
    .gpio_speed = UNI_HAL_GPIO_SPEED_2,
};

uni_hal_gpio_pin_context_t g_ox_trx_pin_spi_miso = {
    .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_E,
    .gpio_pin = UNI_HAL_GPIO_PIN_13,
    .gpio_speed = UNI_HAL_GPIO_SPEED_2,
};

uni_hal_gpio_pin_context_t g_ox_trx_pin_spi_sck = {
    .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_E,
    .gpio_pin = UNI_HAL_GPIO_PIN_12,
    .gpio_speed = UNI_HAL_GPIO_SPEED_2,
};

uni_hal_gpio_pin_context_t g_ox_trx_pin_spi_nss = {
    .gpio_bank = UNI_HAL_CORE_PERIPH_GPIO_E,
    .gpio_pin = UNI_HAL_GPIO_PIN_11,
    .gpio_speed = UNI_HAL_GPIO_SPEED_2,
};

// DMA
uni_hal_dma_context_t g_ox_trx_spi_dma_tx = {
    .config = {
        .instance = UNI_HAL_CORE_PERIPH_DMA_1,
        .channel = UNI_HAL_DMA_CHANNEL_3,
    }
};

// SPI
uni_hal_spi_context_t g_ox_trx_spi_ctx = {
    .config = {
        .instance = UNI_HAL_CORE_PERIPH_SPI_4,
        .clock_source = UNI_HAL_RCC_CLKSRC_PCLK2,
        .prescaler = UNI_HAL_SPI_PRESCALER_4,
        .phase = UNI_HAL_SPI_CPHA_0,
        .polarity = UNI_HAL_SPI_CPOL_0,
        .dma_tx = &g_ox_trx_spi_dma_tx,
        .pin_miso = &g_ox_trx_pin_spi_miso,
        .pin_mosi = &g_ox_trx_pin_spi_mosi,
        .pin_nss = &g_ox_trx_pin_spi_nss,
        .pin_sck = &g_ox_trx_pin_spi_sck,
        .nss_hard = false,
    }
};

// Буферы
ox_trx_datamsg_t UNI_COMMON_COMPILER_ALIGN(32) 
    __attribute__((section(".RAM_D1"))) g_ox_trx_data_buf[OX_TRX_BUF_CNT];
UNI_COMMON_ARRAY_DEFINITION(g_ox_trx_data);

// Контекст
ox_trx_context_t g_ox_trx_ctx = {
    .config = {
        .spi = &g_ox_trx_spi_ctx,
        .buf = &g_ox_trx_data_ctx,
    }
};