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

//
// Private
//

static uint32_t g_app_flash_write_off = 0;
static uint8_t g_app_flash_buf[FLASH_WRITE_ALIGN];
static size_t g_app_flash_buf_fill = 0;

static size_t _ox_server_updater_handler_upload(void* userdata, uint8_t* buf_out, size_t buf_out_size, const uint8_t* buf_in, size_t buf_in_len)
{
    size_t result = 0U;
    (void)userdata;

    if (buf_in != NULL && g_app_flash_write_off == 0) {
        uni_hal_flash_erase_bank();

        if (!g_aes_ccm_initialized) {
            mbedtls_ccm_init(&g_aes_ccm_ctx);
            if (mbedtls_ccm_setkey(&g_aes_ccm_ctx, MBEDTLS_CIPHER_ID_AES, g_ox_crypto_key, AES_KEY_SIZE * 8) != 0) {
                return 0;
            }
            g_aes_ccm_initialized = true;
            g_block_counter = 0;
        }
    }

    // In case buf_out != nullptr -> we are finished -> write total count of bytes
    if (buf_out != NULL && buf_out_size > 0)
    {
        int true_size =  g_app_flash_write_off + g_app_flash_buf_fill;

        if (g_app_flash_buf_fill) {
            memset(&g_app_flash_buf[g_app_flash_buf_fill], 0xFF, FLASH_WRITE_ALIGN - g_app_flash_buf_fill);
            if (uni_hal_flash_write(FLASH_START_ADDR + g_app_flash_write_off, FLASH_WRITE_ALIGN, g_app_flash_buf) != FLASH_WRITE_ALIGN) {
                //uni_hal_io_stdio_printf("Flash write failed at offset %d\r\n", g_app_flash_write_off);
            }
        }
        
        g_app_flash_buf_fill  = 0;
        g_app_flash_write_off = 0;
        g_decrypt_buf_fill = 0;
        g_block_counter = 0;

        if (true_size > (1 * 1024 * 1024)) {
            result = uni_hal_io_stdio_snprintf((char*)buf_out, buf_out_size, "FAIL: Invalid firmware size");
        } else {
            result = uni_hal_io_stdio_snprintf((char*)buf_out, buf_out_size, "%d", true_size);
        }
    }
    // Buf_in != nullptr -> in progress -> write to flash
    else if (buf_in != NULL && buf_in_len > 0)
    {
        size_t in_idx = 0;

        if (g_decrypt_buf_fill > 0) {
            size_t need = uni_common_math_min(AES_BLOCK_SIZE + AES_NONCE_SIZE + AES_MAC_SIZE - g_decrypt_buf_fill, buf_in_len);
            memcpy(&g_decrypt_buf[g_decrypt_buf_fill], &buf_in[in_idx], need);
            g_decrypt_buf_fill += need;
            in_idx += need;

            if (g_decrypt_buf_fill == AES_BLOCK_SIZE + AES_NONCE_SIZE + AES_MAC_SIZE) {
                uint8_t nonce[AES_NONCE_SIZE];
                uint8_t mac[AES_MAC_SIZE];
                uint8_t decrypted[AES_BLOCK_SIZE];

                memcpy(nonce, &g_decrypt_buf[0], AES_NONCE_SIZE);
                memcpy(&decrypted[0], &g_decrypt_buf[AES_NONCE_SIZE], AES_BLOCK_SIZE);
                memcpy(mac, &g_decrypt_buf[AES_NONCE_SIZE + AES_BLOCK_SIZE], AES_MAC_SIZE);

                if (mbedtls_ccm_auth_decrypt(&g_aes_ccm_ctx, AES_BLOCK_SIZE, nonce, AES_NONCE_SIZE,
                                             NULL, 0, &decrypted[0], &decrypted[0], mac, AES_MAC_SIZE) != 0) {
                    return 0;
                }

                size_t flash_need = uni_common_math_min(FLASH_WRITE_ALIGN - g_app_flash_buf_fill, AES_BLOCK_SIZE);
                memcpy(&g_app_flash_buf[g_app_flash_buf_fill], decrypted, flash_need);
                g_app_flash_buf_fill += flash_need;

                if (g_app_flash_buf_fill == FLASH_WRITE_ALIGN) {
                    if (uni_hal_flash_write(FLASH_START_ADDR + g_app_flash_write_off, FLASH_WRITE_ALIGN, g_app_flash_buf) != FLASH_WRITE_ALIGN) {
                        return 0;
                    }
                    g_app_flash_write_off += FLASH_WRITE_ALIGN;
                    g_app_flash_buf_fill = 0;

                    if (flash_need < AES_BLOCK_SIZE) {
                        memcpy(g_app_flash_buf, &decrypted[flash_need], AES_BLOCK_SIZE - flash_need);
                        g_app_flash_buf_fill = AES_BLOCK_SIZE - flash_need;
                    }
                }

                g_decrypt_buf_fill = 0;
                g_block_counter++;
            }
        }

        portYIELD();

        size_t encrypted_block_size = AES_BLOCK_SIZE + AES_NONCE_SIZE + AES_MAC_SIZE;
        while (in_idx + encrypted_block_size <= buf_in_len) {
            uint8_t nonce[AES_NONCE_SIZE];
            uint8_t mac[AES_MAC_SIZE];
            uint8_t decrypted[AES_BLOCK_SIZE];

            memcpy(nonce, &buf_in[in_idx], AES_NONCE_SIZE);
            memcpy(&decrypted[0], &buf_in[in_idx + AES_NONCE_SIZE], AES_BLOCK_SIZE);
            memcpy(mac, &buf_in[in_idx + AES_NONCE_SIZE + AES_BLOCK_SIZE], AES_MAC_SIZE);

            if (mbedtls_ccm_auth_decrypt(&g_aes_ccm_ctx, AES_BLOCK_SIZE, nonce, AES_NONCE_SIZE,
                                         NULL, 0, &decrypted[0], &decrypted[0], mac, AES_MAC_SIZE) != 0) {
                return 0;
            }

            in_idx += encrypted_block_size;

            size_t decrypt_idx = 0;
            while (decrypt_idx < AES_BLOCK_SIZE) {
                size_t flash_need = uni_common_math_min(FLASH_WRITE_ALIGN - g_app_flash_buf_fill, AES_BLOCK_SIZE - decrypt_idx);
                memcpy(&g_app_flash_buf[g_app_flash_buf_fill], &decrypted[decrypt_idx], flash_need);
                g_app_flash_buf_fill += flash_need;
                decrypt_idx += flash_need;

                if (g_app_flash_buf_fill == FLASH_WRITE_ALIGN) {
                    if (uni_hal_flash_write(FLASH_START_ADDR + g_app_flash_write_off, FLASH_WRITE_ALIGN, g_app_flash_buf) != FLASH_WRITE_ALIGN) {
                        return 0;
                    }
                    g_app_flash_write_off += FLASH_WRITE_ALIGN;
                    g_app_flash_buf_fill = 0;
                }
            }

            g_block_counter++;
        }

        portYIELD();
    }
    return result;
}

static size_t _ox_server_updater_handler_download(void* userdata, uint8_t* buf_out, size_t buf_out_size, const uint8_t* buf_in, size_t buf_in_len)
{
    static size_t g_download_offset = 0;
    static const size_t FIRMWARE_SIZE = 1 * 1024 * 1024; // 1 MiB

    size_t result = 0U;
    (void)userdata;
    (void)buf_in_len;

    if (buf_in != NULL) {
        g_download_offset = 0;
        return 0;
    }

    if (buf_out != NULL && buf_out_size > 0) {
        size_t remaining = FIRMWARE_SIZE - g_download_offset;

        if (remaining == 0) {
            g_download_offset = 0;
            return 0;
        }

        size_t to_read = uni_common_math_min(buf_out_size, remaining);

        const uint8_t* flash_ptr = (const uint8_t*)(FLASH_START_ADDR + FLASH_BANK_SIZE);
        memcpy(buf_out, flash_ptr, to_read);
        g_download_offset += to_read;
        result = to_read;


        portYIELD();
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

    result &= uni_net_http_server_register_file_ex(server, "/updater.html", g_ox_asset_updater, g_ox_asset_updater_size);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_GET, "/updater/status", _ox_server_updater_handler_status, ctx);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_GET, "/updater/apply", _ox_server_updater_handler_apply, ctx);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_POST, "/updater/upload", _ox_server_updater_handler_upload, ctx);
    result &= uni_net_http_server_register_handler_ex(server, UNI_NET_HTTP_COMMAND_GET, "/updater/download", _ox_server_updater_handler_download, ctx);
    return result;
}
