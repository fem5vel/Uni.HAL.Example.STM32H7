//
// Includes
//

// FreeRTOS
#include <FreeRTOS_IP.h>

// ox
#include "io/uni_hal_io_stdio.h"
#include "rng/ox_rng.h"


//
// Hooks/IP
//

void vApplicationIPNetworkEventHook_Multi( eIPCallbackEvent_t eNetworkEvent,
                                           struct xNetworkEndPoint * pxEndPoint ) {
    (void) pxEndPoint;

    switch (eNetworkEvent) {
        case eNetworkUp:
            break;
        case eNetworkDown:
            break;
        default:
            break;
    }
}


uint32_t ulApplicationGetNextSequenceNumber( uint32_t , uint16_t , uint32_t , uint16_t  ) {
    uint32_t result = 0U;
    xApplicationGetRandomNumber(&result);
    return result;
}



//
// Hooks/DHCP
//

const char * pcApplicationHostnameHook( void ){
    return "STM32 H7";
}

eDHCPCallbackAnswer_t xApplicationDHCPHook_Multi( eDHCPCallbackPhase_t phase,struct xNetworkEndPoint * endpoint, IP_Address_t *  address) {
    (void)phase;
    (void)endpoint;
    (void)address;
    return eDHCPContinue;
}



//
// Hooks/Random
//

BaseType_t xApplicationGetRandomNumber( uint32_t *pulValue ) {
    BaseType_t xReturn = pdFAIL;
    if (pulValue != nullptr) {
        *pulValue = ox_rng_get_u32();
        xReturn = pdTRUE;
    }
    return xReturn;
}
