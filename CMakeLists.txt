#
# Toolchain
#

if(NOT DEFINED UNI_HAL_HSE_VALUE)
    set(UNI_HAL_HSE_VALUE 8000000)
endif()
if(NOT DEFINED UNI_HAL_TARGET_MCU)
    set(UNI_HAL_TARGET_MCU "STM32H743")
endif()
if(NOT DEFINED UNI_HAL_RTOS_HEAP_APP)
    set(UNI_HAL_RTOS_HEAP_APP 1)
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/src_unihal/cmake/toolchain.cmake")



#
# Project
#

cmake_minimum_required(VERSION 3.26)

project(uni_hal_example_stm32h7 LANGUAGES C ASM)



#
# Defines
#

# device

add_compile_definitions(TARGET_DEVICE_NUCLEO)



# Heap
set(UNI_HAL_RTOS_HEAP_SIZE 300*1024)


#
# Uni.HAL
#
add_subdirectory(src_unicommon)
add_subdirectory(src_unihal)
include(src_unihal/uni.hal.cmake)
add_subdirectory(src_uninet)


#
# Dependencies
#

find_package(Python COMPONENTS Interpreter REQUIRED)

include(cmake/cpm.cmake)
include(cmake/cpm-mbedtls.cmake)



#
# App
#

add_subdirectory(src_assets)
add_subdirectory(src_lib)
add_subdirectory(src_app)
