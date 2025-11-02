# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(bitdoglab-arduino-uart C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Adicione antes do add_subdirectory(FreeRTOS-Kernel):
# Configuração do FreeRTOS
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/config  # Aponta para a pasta config onde está o FreeRTOSConfig.h
)
target_compile_definitions(freertos_config INTERFACE
    projCOVERAGE_TEST=0
    configUSE_RECURSIVE_MUTEXES=1
    INCLUDE_xSemaphoreGetMutexHolder=1
)

# Define a porta do FreeRTOS para RP2040
set(FREERTOS_PORT GCC_RP2040 CACHE STRING "Port for FreeRTOS")
add_subdirectory(FreeRTOS-Kernel)

# Add executable. Default name is the project name, version 0.1

add_executable(${PROJECT_NAME} 
    main.c 
    hw_config.c
    mqtt_publisher_task.c
    lib_ssd1306/ssd1306.c
    lib_ssd1306/ssd1306_fonts.c
    lib_ssd1306/ssd1306_bitmaps.c
    FreeRTOS-Kernel/portable/MemMang/heap_4.c
    )


add_subdirectory(lib/FatFs_SPI)

pico_set_program_name(${PROJECT_NAME} "bitdoglab-arduino-uart")
pico_set_program_version(${PROJECT_NAME} "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/lib_ssd1306
    ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include
    ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_rand/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_lwip/include
    ${PICO_SDK_PATH}/lib/lwip/src/include
    ${PICO_SDK_PATH}/lib/cyw43-driver/src                # Adiciona include do driver CYW43
    ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
    ${PICO_SDK_PATH}/lib/lwip/contrib/ports/freertos/include
)

# Remova a definição duplicada de PICO_BOARD do target_compile_definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Remover PICO_BOARD=pico_w daqui
    CYW43_HOST_NAME="PicoW"
    NO_SYS=1
    LWIP_NETCONN=0
    LWIP_SOCKET=0
    LWIP_PROVIDE_ERRNO=1
    MEM_LIBC_MALLOC=0
    LWIP_DNS=1
    LWIP_DHCP=1
    CYW43_LWIP=1
)

# Ajuste a ordem das bibliotecas para resolver as dependências
target_link_libraries(${PROJECT_NAME}
    # WiFi e rede (ordem é importante!)
    pico_cyw43_arch_lwip_threadsafe_background
    pico_cyw43_driver
    pico_cyw43_arch
    
    # Core do sistema
    pico_stdlib
    hardware_adc
    
    # lwIP e suas dependências
    pico_lwip_nosys
    pico_lwip
    
    # FreeRTOS
    freertos_kernel
    freertos_config
    
    # Hardware
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_clocks 
    hardware_dma
    hardware_rtc
    
    # Utilitários
    pico_sync
    pico_time
    pico_rand
    
    # FAT FS
    FatFs_SPI
)

# Modifique as definições do lwIP
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CYW43_HOST_NAME="PicoW"
    NO_SYS=1
    LWIP_NETCONN=0
    LWIP_SOCKET=0
    LWIP_PROVIDE_ERRNO=1
    MEM_LIBC_MALLOC=0
    LWIP_DNS=1
    LWIP_DHCP=1
    CYW43_LWIP=1
    PICO_CYW43_ARCH_POLL=0
)

pico_add_extra_outputs(${PROJECT_NAME})
target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--print-memory-usage
    )
