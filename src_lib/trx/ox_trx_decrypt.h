#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ox_trx.h"

typedef struct {
    void* aes_ctx;
    uint8_t* decrypt_buf;
    size_t decrypt_buf_fill;
    uint64_t block_counter;
    bool initialized;
} ox_trx_decrypt_context_t;

bool ox_trx_decrypt_init(ox_trx_decrypt_context_t* ctx);
bool ox_trx_decrypt_process(ox_trx_decrypt_context_t* ctx, ox_trx_context_t* trx_ctx,
                             const uint8_t* buf_in, size_t buf_in_len);
void ox_trx_decrypt_deinit(ox_trx_decrypt_context_t* ctx);
