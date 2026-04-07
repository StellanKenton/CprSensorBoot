cmake_minimum_required(VERSION 3.21)

project(CprSensorBoot LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TARGET_NAME CprSensorBoot)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103re_bootloader.ld)
set(MAP_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.map)

set(PROJECT_SOURCES
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/adc.c
    Core/Src/dma.c
    Core/Src/iwdg.c
    Core/Src/rtc.c
    Core/Src/spi.c
    Core/Src/tim.c
    Core/Src/usart.c
    Core/Src/stm32f1xx_it.c
    Core/Src/stm32f1xx_hal_msp.c
    Core/Src/stm32f1xx_hal_timebase_tim.c
    Core/Src/system_stm32f1xx.c

    USB_DEVICE/App/usb_device.c
    USB_DEVICE/App/usbd_desc.c
    USB_DEVICE/App/usbd_cdc_if.c
    USB_DEVICE/Target/usbd_conf.c

    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_iwdg.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rtc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rtc_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c

    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c

    User/App/app_system.c
    User/Drv/drv_wdg.c
    User/Drv/drv_delay.c
    User/Lib/lib_comm.c
    User/Lib/MD5.c
    User/Lib/encryption.c
    User/Bsp/bspspi.c
    User/Bsp/bsp_uart.c
    User/SEGGER/bsp_rtt.c
    User/SEGGER/SEGGER_RTT.c
    User/SEGGER/SEGGER_RTT_printf.c
    User/SEGGER/SEGGER_RTT_ASM_ARMv7M.S
    User/BackTrace/cm_backtrace.c
    User/BackTrace/fault_handler/gcc/cmb_fault.S

    User/rep/console/console.c
    User/rep/console/log.c
    User/rep/console/log_port.c
    User/rep/ringbuffer/ringbuffer.c
    User/rep/drvlayer/drvspi/drvspi.c
    User/rep/drvlayer/drvspi/drvspi_port.c
    User/rep/drvlayer/drvspi/drvspi_debug.c
    User/rep/drvlayer/drvuart/drvuart.c
    User/rep/drvlayer/drvuart/drvuart_port.c
    User/rep/module/w25qxxx/w25qxxx.c
    User/rep/module/w25qxxx/w25qxxx_port.c
    User/rep/module/w25qxxx/w25qxxx_debug.c

    Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xe.s
)

set(PROJECT_INCLUDES
    Core/Inc
    USB_DEVICE/App
    USB_DEVICE/Target
    Drivers/STM32F1xx_HAL_Driver/Inc
    Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
    Drivers/CMSIS/Device/ST/STM32F1xx/Include
    Drivers/CMSIS/Include
    User
    User/App
    User/BackTrace
    User/Bsp
    User/Drv
    User/Lib
    User/SEGGER
    User/rep
    User/rep/console
    User/rep/ringbuffer
    User/rep/drvlayer/drvspi
    User/rep/drvlayer/drvuart
    User/rep/module/w25qxxx
)

add_executable(${TARGET_NAME} ${PROJECT_SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE ${PROJECT_INCLUDES})

target_compile_definitions(${TARGET_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F103xE
    REP_MCU_PLATFORM=REP_MCU_PLATFORM_STM32
    REP_RTOS_SYSTEM=REP_RTOS_NONE
)

target_compile_options(${TARGET_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -ffunction-sections
    -fdata-sections
    -fno-common
    $<$<COMPILE_LANGUAGE:C>:-fmessage-length=0>
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-Os -g0>
)

target_link_options(${TARGET_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -specs=nano.specs
    -specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${MAP_FILE}
    -T${LINKER_SCRIPT}
)

target_link_libraries(${TARGET_NAME} PRIVATE c m nosys)

set(ELF_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.elf)
set(HEX_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${BIN_FILE}
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    BYPRODUCTS ${HEX_FILE} ${BIN_FILE}
    COMMENT "Generating ${TARGET_NAME}.hex and ${TARGET_NAME}.bin"
)