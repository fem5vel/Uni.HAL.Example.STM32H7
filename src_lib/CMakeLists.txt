#
# Library
#

uni_hal_add_library(uni_hal_example_stm32h7_lib)

# sources
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "*.c")
target_sources(uni_hal_example_stm32h7_lib PRIVATE ${SRCS})

target_sources(uni_hal_example_stm32h7_lib INTERFACE net/ox_net_hooks.c)

# include
target_include_directories(uni_hal_example_stm32h7_lib PUBLIC .)

# linl
target_link_libraries(uni_hal_example_stm32h7_lib PUBLIC uni.net)
target_link_libraries(uni_hal_example_stm32h7_lib PUBLIC uni_hal_example_stm32h7_assets)
target_link_libraries(uni_hal_example_stm32h7_lib PUBLIC mbedcrypto)
target_link_libraries(uni_hal_example_stm32h7_lib PUBLIC corejson)
target_link_libraries(uni_hal_example_stm32h7_lib PUBLIC uni_crypto)
