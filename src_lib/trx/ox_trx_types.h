#pragma once

//
// Includes
//

// stdlib
#include <stdint.h>

// uni.common
#include "uni_common_compiler.h"


//
// Typedefs/Defines
//

#define OX_TRX_MSG_SIZE     (1440U)
#define OX_TRX_BUF_CNT (150U)

//
// Typedefs/Structs
//

typedef struct UNI_COMMON_COMPILER_ALIGN_PACK(1) {
    uint8_t data[OX_TRX_MSG_SIZE];
} ox_trx_datamsg_t;

static_assert(sizeof(ox_trx_datamsg_t) % 32 == 0);

