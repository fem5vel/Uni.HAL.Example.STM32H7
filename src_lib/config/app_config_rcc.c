//
// Includes
//

// ox
#include "config/app_config_rcc.h"



//
// Globals
//

#if defined(STM32H7)

uni_hal_rcc_stm32h7_config_t g_ox_rcc_config = {
        .hse_enable = false,
#if defined(TARGET_DEVICE_NUCLEO)
        .hse_bypass = true,
#else
        .hse_bypass = false,
#endif

        .hse_css = true,
        .csi_enable = false,
        .lse_enable = false,

        .pll[0] = {
                .m = 2,
                .n = 200,
                .p = 2,
                .q = 4,
                .r = 0,
        },
        .pll[1] = {
                .m = 2,
                .n = 100,
                .p = 8,
                .q = 4,
                .r = 0,
        },
        .pll[2] = {
                .m = 0,
                .n = 0,
                .p = 0,
                .q = 0,
                .r = 0,
        },
        .timeout = {
                .hse = 250,
                .hsi = 250,
                .lse = 250,
                .lsi = 250,
                .pll = 250
        }
};

#endif
