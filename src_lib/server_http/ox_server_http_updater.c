//
// Includes
//

// stdlib
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


// uni.hal
#include <uni_common.h>
#include <uni_hal.h>

// ox
#include "server_http/ox_server_http_updater.h"

#include <config/app_config_crypto.h>

#include "fwinfo/ox_fwinfo.h"
#include "ox_asset_updater.h"
#include <mbedtls/ccm.h>
#include "trx/ox_trx_decrypt.h"
#include "trx/app_config_trx.h"
#include "perf/ox_perf_stats.h"

//
// Defines
//

#if defined(UNI_HAL_TARGET_MCU_PC)
#define FLASH_BASE 0
#define FLASH_BANK_SIZE (1024*1024)
#endif

#define FLASH_WRITE_ALIGN (32)
#define FLASH_START_ADDR (FLASH_BASE + FLASH_BANK_SIZE)
#define FLASH_END_ADDR (FLASH_BASE + FLASH_BANK_SIZE)

#define AES_KEY_SIZE 32
#define AES_NONCE_SIZE 8
#define AES_MAC_SIZE 8
#define AES_BLOCK_SIZE 32

static mbedtls_ccm_context g_aes_ccm_ctx;
static bool g_aes_ccm_initialized = false;
static uint8_t g_decrypt_buf[AES_BLOCK_SIZE + AES_NONCE_SIZE + AES_MAC_SIZE];
static size_t g_decrypt_buf_fill = 0;
static uint64_t g_block_counter = 0;

static ox_trx_decrypt_context_t g_decrypt_ctx = {0};

static ox_perf_counter_t g_perf_decrypt = {0};
static ox_perf_counter_t g_perf_flash = {0};
static ox_perf_counter_t g_perf_total = {0};
static ox_perf_counter_t g_perf_spi_decrypt = {0};
static ox_perf_counter_t g_perf_spi_queue = {0};
static ox_perf_counter_t g_perf_spi_total = {0};

#ifndef CPU_FREQ_HZ
#define CPU_FREQ_HZ 400000000U
#endif
//
// Private
//

static uint32_t g_app_flash_write_off = 0;
static uint8_t g_app_flash_buf[FLASH_WRITE_ALIGN];
static size_t g_app_flash_buf_fill = 0;

static size_t _ox_server_updater_handler_upload(void* userdata, uint8_t* buf_out,
                                                 size_t buf_out_size, const uint8_t* buf_in,
                                                 size_t buf_in_len)
{
    size_t result = 0U;
    (void)userdata;

    // Начало первого вызова - инициализация
    if (buf_in != NULL && g_app_flash_write_off == 0 && g_app_flash_buf_fill == 0) {
        if (!g_aes_ccm_initialized) {
            mbedtls_ccm_init(&g_aes_ccm_ctx);
            if (mbedtls_ccm_setkey(&g_aes_ccm_ctx, MBEDTLS_CIPHER_ID_AES,
                                   g_ox_crypto_key, AES_KEY_SIZE * 8) != 0) {
                return 0;
            }
            g_aes_ccm_initialized = true;
        }

        g_decrypt_buf_fill = 0;
        g_block_counter = 0;

        ox_perf_counter_reset(&g_perf_decrypt);
        ox_perf_counter_reset(&g_perf_flash);
        ox_perf_counter_reset(&g_perf_total);

        uni_hal_io_stdio_printf("[UPLOAD] Starting firmware upload with performance monitoring\r\n");

        uni_hal_flash_erase_bank();
    }

    if (buf_out != NULL && buf_out_size > 0)
    {
        int true_size = g_app_flash_write_off + g_app_flash_buf_fill;

        if (g_app_flash_buf_fill > 0)
        {
            ox_perf_counter_start(&g_perf_flash);
            memset(&g_app_flash_buf[g_app_flash_buf_fill], 0xFF, FLASH_WRITE_ALIGN - g_app_flash_buf_fill);
            if (uni_hal_flash_write(FLASH_START_ADDR + g_app_flash_write_off,
                                    FLASH_WRITE_ALIGN, g_app_flash_buf) != FLASH_WRITE_ALIGN)
            {
                // Ошибка записи
            }
            ox_perf_counter_stop(&g_perf_flash, FLASH_WRITE_ALIGN);
        }

        uni_hal_io_stdio_printf("\r\n[UPLOAD] === Upload Complete ===\r\n");
        ox_perf_counter_print(&g_perf_decrypt, "DECRYPT", CPU_FREQ_HZ);
        ox_perf_counter_print(&g_perf_flash, "FLASH_WRITE", CPU_FREQ_HZ);
        ox_perf_counter_print(&g_perf_total, "TOTAL_PROCESSING", CPU_FREQ_HZ);

        g_app_flash_buf_fill = 0;
        g_app_flash_write_off = 0;
        g_decrypt_buf_fill = 0;
        g_block_counter = 0;

        result = (size_t)snprintf((char*)buf_out, buf_out_size, "%d", true_size);
    }
    // Обработка входящих данных
    else if (buf_in != NULL && buf_in_len > 0)
    {
        // Начало измерения общего времени обработки пакета
        ox_perf_counter_start(&g_perf_total);

        const size_t ENCRYPTED_BLOCK_SIZE = AES_NONCE_SIZE + AES_BLOCK_SIZE + AES_MAC_SIZE;
        size_t buf_in_offset = 0;
        size_t bytes_decrypted_this_call = 0;
        size_t bytes_written_this_call = 0;

        while (buf_in_offset < buf_in_len) {
            size_t bytes_needed = ENCRYPTED_BLOCK_SIZE - g_decrypt_buf_fill;
            size_t bytes_available = buf_in_len - buf_in_offset;
            size_t bytes_to_copy = (bytes_available < bytes_needed) ? bytes_available : bytes_needed;

            memcpy(&g_decrypt_buf[g_decrypt_buf_fill], &buf_in[buf_in_offset], bytes_to_copy);
            g_decrypt_buf_fill += bytes_to_copy;
            buf_in_offset += bytes_to_copy;

            if (g_decrypt_buf_fill == ENCRYPTED_BLOCK_SIZE) {
                uint8_t* nonce = &g_decrypt_buf[0];
                uint8_t* ciphertext = &g_decrypt_buf[AES_NONCE_SIZE];
                uint8_t* mac = &g_decrypt_buf[AES_NONCE_SIZE + AES_BLOCK_SIZE];
                uint8_t plaintext[AES_BLOCK_SIZE];

                ox_perf_counter_start(&g_perf_decrypt);

                int ret = mbedtls_ccm_auth_decrypt(&g_aes_ccm_ctx,
                                                   AES_BLOCK_SIZE,
                                                   nonce, AES_NONCE_SIZE,
                                                   NULL, 0,
                                                   ciphertext, plaintext,
                                                   mac, AES_MAC_SIZE);

                ox_perf_counter_stop(&g_perf_decrypt, AES_BLOCK_SIZE);

                if (ret != 0) {
                    g_decrypt_buf_fill = 0;
                    return 0;
                }

                g_block_counter++;
                g_decrypt_buf_fill = 0;
                bytes_decrypted_this_call += AES_BLOCK_SIZE;

                for (size_t i = 0; i < AES_BLOCK_SIZE; i++) {
                    g_app_flash_buf[g_app_flash_buf_fill++] = plaintext[i];

                    if (g_app_flash_buf_fill == FLASH_WRITE_ALIGN) {
                        ox_perf_counter_start(&g_perf_flash);

                        if (uni_hal_flash_write(FLASH_START_ADDR + g_app_flash_write_off,
                                               FLASH_WRITE_ALIGN, g_app_flash_buf) != FLASH_WRITE_ALIGN) {
                            return 0;
                        }

                        ox_perf_counter_stop(&g_perf_flash, FLASH_WRITE_ALIGN);

                        g_app_flash_write_off += FLASH_WRITE_ALIGN;
                        g_app_flash_buf_fill = 0;
                        bytes_written_this_call += FLASH_WRITE_ALIGN;
                    }
                }
            }
        }

        ox_perf_counter_stop(&g_perf_total, buf_in_len);

        result = buf_in_len;
    }

    return result;
}

static size_t _ox_server_updater_handler_spi(void* userdata, uint8_t* buf_out,
                                              size_t buf_out_size, const uint8_t* buf_in,
                                              size_t buf_in_len) {
    size_t result = 0U;
    (void)userdata;

    if (buf_in != NULL && g_decrypt_ctx.initialized == false) {
        uni_hal_io_stdio_printf("[SPI] Decrypt context initialization...\r\n");
        if (!ox_trx_decrypt_init(&g_decrypt_ctx)) {
            uni_hal_io_stdio_printf("[SPI] ERROR: Failed to initialize decryption context\r\n");
            return 0;
        }

        ox_perf_counter_reset(&g_perf_spi_decrypt);
        ox_perf_counter_reset(&g_perf_spi_queue);
        ox_perf_counter_reset(&g_perf_spi_total);

        uni_hal_io_stdio_printf("[SPI] Decrypt context initialized with performance monitoring\r\n");
    }

    if (buf_out != NULL && buf_out_size > 0) {
        uni_hal_io_stdio_printf("[SPI] Waiting for SPI transfers to complete...\r\n");

        int timeout = 0;
        while ((uni_hal_spi_is_busy(g_ox_trx_ctx.config.spi) ||
               g_ox_trx_ctx.state.in_transfer != NULL) && timeout < 1000) {
            taskYIELD();
            timeout++;
        }

        if (timeout >= 1000) {
            uni_hal_io_stdio_printf("[SPI] WARNING: SPI timeout exceeded\r\n");
        }

        uni_hal_io_stdio_printf("\r\n[SPI] === SPI Transfer Complete ===\r\n");
        ox_perf_counter_print(&g_perf_spi_decrypt, "SPI_DECRYPT", CPU_FREQ_HZ);
        ox_perf_counter_print(&g_perf_spi_queue, "SPI_QUEUE_SEND", CPU_FREQ_HZ);
        ox_perf_counter_print(&g_perf_spi_total, "SPI_TOTAL", CPU_FREQ_HZ);

        ox_trx_decrypt_deinit(&g_decrypt_ctx);

        uint64_t bytes_transmitted = (uint64_t)g_decrypt_ctx.block_counter * AES_BLOCK_SIZE;

        uint32_t blocks = (uint32_t)g_decrypt_ctx.block_counter;
        uint32_t bytes_low = (uint32_t)(bytes_transmitted & 0xFFFFFFFFUL);

        uni_hal_io_stdio_printf("[SPI] Transfer complete. Total blocks: %lu, bytes: %lu\r\n",
                                (unsigned long)blocks,
                                (unsigned long)bytes_low);

        result = (size_t)snprintf((char*)buf_out, buf_out_size, "%lu", (unsigned long)bytes_low);
    }
    else if (buf_in != NULL && buf_in_len > 0) {
        uni_hal_io_stdio_printf("[SPI] Received %u bytes of encrypted data\r\n", (unsigned int)buf_in_len);

        ox_perf_counter_start(&g_perf_spi_total);

        if (ox_trx_decrypt_process_with_perf(&g_decrypt_ctx, &g_ox_trx_ctx, buf_in, buf_in_len,
                                              &g_perf_spi_decrypt, &g_perf_spi_queue)) {
            uni_hal_io_stdio_printf("[SPI] Data processed successfully. Block counter: %lu\r\n",
                                    (unsigned long)g_decrypt_ctx.block_counter);
            result = buf_in_len;
        } else {
            uni_hal_io_stdio_printf("[SPI] ERROR: Failed to process data\r\n");
        }

        ox_perf_counter_stop(&g_perf_spi_total, buf_in_len);
    }

    return result;
}

static size_t _ox_server_updater_handler_apply(void* userdata, uint8_t* buf_out, size_t buf_out_size, const uint8_t* buf_in, size_t buf_in_len)
{
    (void)userdata;
    (void)buf_in;
    (void)buf_in_len;

    portYIELD();
    if (ox_fwinfo_is_valid(OX_FWINFO_NEXT)) {
        uni_hal_flash_swap_banks();
        return uni_hal_io_stdio_snprintf((char*)buf_out, buf_out_size, "OK");
    } else {
        return uni_hal_io_stdio_snprintf((char*)buf_out, buf_out_size, "FAIL: HMAC is not valid");
    }
}

static size_t _ox_server_updater_handler_status(void* userdata, uint8_t* buf_out, size_t buf_out_size, const uint8_t* buf_in, size_t buf_in_len)
{
    (void)userdata;
    (void)buf_in;
    (void)buf_in_len;

    // current
    char current_ver_str[64 + 1] = {0};
    bool is_current_valid = ox_fwinfo_is_valid(OX_FWINFO_CURRENT);
    if (is_current_valid) {
        const ox_fwinfo_t* fw_info_current = ox_fwinfo_get_info(OX_FWINFO_CURRENT);
        strcpy(current_ver_str, fw_info_current->git_version);
    }

    // next
    char next_ver_str[64 + 1] = {0};
    bool is_next_valid = ox_fwinfo_is_valid(OX_FWINFO_NEXT);
    if (is_next_valid) {
        const ox_fwinfo_t* fw_info_next = ox_fwinfo_get_info(OX_FWINFO_NEXT);
        strcpy(next_ver_str, fw_info_next->git_version);
    }

    return uni_hal_io_stdio_snprintf((char*)buf_out, buf_out_size,
                    "{\"current_firmware\": {\"version\": \"%s\", \"hmac_valid\": %s}, "
                    "\"next_firmware\": {\"version\": \"%s\", \"hmac_valid\": %s}}",
                    current_ver_str, is_current_valid ? "true" : "false",
                    next_ver_str, is_next_valid ? "true" : "false");
}


//
// Implementation
//

bool ox_server_updater_init(ox_server_updater_context_t* ctx, uni_net_http_server_context_t* server) {
    bool result = 1;

    static uni_net_http_file_t firmware_file = {
        .path = "/updater/download",
        .data = NULL,
        .size = 1 * 1024 * 1024
    };

    if (firmware_file.data == NULL) {
        firmware_file.data = (const uint8_t*)FLASH_BANK2_BASE;
    }

    result &= uni_net_http_server_register_file(server, &firmware_file);
    result &= uni_net_http_server_register_file_ex(server, "/updater.html", g_ox_asset_updater, g_ox_asset_updater_size);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_GET, "/updater/status", _ox_server_updater_handler_status, ctx);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_GET, "/updater/apply", _ox_server_updater_handler_apply, ctx);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_POST, "/updater/upload", _ox_server_updater_handler_upload, ctx);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_POST, "/updater/spi", _ox_server_updater_handler_spi, ctx);
    return result;
}
