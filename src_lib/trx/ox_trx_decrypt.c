#include <string.h>
#include <mbedtls/ccm.h>
#include "ox_trx_decrypt.h"
#include <config/app_config_crypto.h>
#include <stdlib.h>

#define AES_KEY_SIZE 32
#define AES_NONCE_SIZE 8
#define AES_MAC_SIZE 8
#define AES_BLOCK_SIZE 32
#define ENCRYPTED_BLOCK_SIZE (AES_NONCE_SIZE + AES_BLOCK_SIZE + AES_MAC_SIZE)

static mbedtls_ccm_context* _get_aes_ctx(ox_trx_decrypt_context_t* ctx) {
    return (mbedtls_ccm_context*)ctx->aes_ctx;
}

bool ox_trx_decrypt_init(ox_trx_decrypt_context_t* ctx) {
    if (ctx == NULL) {
        return false;
    }

    mbedtls_ccm_context* aes_ctx = malloc(sizeof(mbedtls_ccm_context));
    if (aes_ctx == NULL) {
        return false;
    }

    uint8_t* decrypt_buf = malloc(ENCRYPTED_BLOCK_SIZE);
    if (decrypt_buf == NULL) {
        free(aes_ctx);
        return false;
    }

    mbedtls_ccm_init(aes_ctx);
    if (mbedtls_ccm_setkey(aes_ctx, MBEDTLS_CIPHER_ID_AES,
                           g_ox_crypto_key, AES_KEY_SIZE * 8) != 0) {
        free(decrypt_buf);
        free(aes_ctx);
        return false;
    }

    ctx->aes_ctx = aes_ctx;
    ctx->decrypt_buf = decrypt_buf;
    ctx->decrypt_buf_fill = 0;
    ctx->block_counter = 0;
    ctx->initialized = true;

    return true;
}

bool ox_trx_decrypt_process(ox_trx_decrypt_context_t* ctx, ox_trx_context_t* trx_ctx,
                             const uint8_t* buf_in, size_t buf_in_len) {
    if (ctx == NULL || !ctx->initialized || buf_in == NULL || buf_in_len == 0) {
        // Ошибка: невалидные параметры функции
        uni_hal_io_stdio_printf("[DECRYPT] ERROR: Invalid parameters\r\n");
        return false;
    }

    mbedtls_ccm_context* aes_ctx = _get_aes_ctx(ctx);
    size_t buf_in_offset = 0;
    size_t blocks_processed = 0;

    while (buf_in_offset < buf_in_len) {
        size_t bytes_needed = ENCRYPTED_BLOCK_SIZE - ctx->decrypt_buf_fill;
        size_t bytes_available = buf_in_len - buf_in_offset;
        size_t bytes_to_copy = (bytes_available < bytes_needed) ? bytes_available : bytes_needed;

        memcpy(&ctx->decrypt_buf[ctx->decrypt_buf_fill], &buf_in[buf_in_offset], bytes_to_copy);
        ctx->decrypt_buf_fill += bytes_to_copy;
        buf_in_offset += bytes_to_copy;

        if (ctx->decrypt_buf_fill == ENCRYPTED_BLOCK_SIZE) {
            uint8_t* nonce = &ctx->decrypt_buf[0];
            uint8_t* ciphertext = &ctx->decrypt_buf[AES_NONCE_SIZE];
            uint8_t* mac = &ctx->decrypt_buf[AES_NONCE_SIZE + AES_BLOCK_SIZE];

            uint8_t plaintext[AES_BLOCK_SIZE];

            int ret = mbedtls_ccm_auth_decrypt(aes_ctx,
                                               AES_BLOCK_SIZE,
                                               nonce, AES_NONCE_SIZE,
                                               NULL, 0,
                                               ciphertext, plaintext,
                                               mac, AES_MAC_SIZE);

            if (ret != 0) {
                // Ошибка дешифрования или проверки MAC
                uni_hal_io_stdio_printf("[DECRYPT] ERROR: Decryption failed (ret=%d)\r\n", ret);
                ctx->decrypt_buf_fill = 0;
                return false;
            }

            ctx->block_counter++;
            ctx->decrypt_buf_fill = 0;
            blocks_processed++;

            ox_trx_datamsg_t* buf = ox_trx_get_free_buffer(trx_ctx);
            if (buf != NULL) {
                memcpy(buf->data, plaintext, AES_BLOCK_SIZE);
                if (!ox_trx_send_buffer(trx_ctx, buf)) {
                    // Ошибка отправки буфера в SPI очередь
                    uni_hal_io_stdio_printf("[DECRYPT] ERROR: Failed to send buffer to SPI queue\r\n");
                    return false;
                }
            } else {
                // Нет свободных буферов
                uni_hal_io_stdio_printf("[DECRYPT] ERROR: No free buffers available\r\n");
                return false;
            }
        }
    }

    if (blocks_processed > 0) {
        // Обработано блоков
        uni_hal_io_stdio_printf("[DECRYPT] Processed %u blocks. Total blocks: %lu\r\n",
                                blocks_processed, (unsigned long)ctx->block_counter);
    }

    return true;
}

void ox_trx_decrypt_deinit(ox_trx_decrypt_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }

    if (ctx->aes_ctx != NULL) {
        mbedtls_ccm_free((mbedtls_ccm_context*)ctx->aes_ctx);
        free(ctx->aes_ctx);
    }

    if (ctx->decrypt_buf != NULL) {
        free(ctx->decrypt_buf);
    }

    ctx->initialized = false;
}
