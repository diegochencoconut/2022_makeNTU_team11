################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/flexspi_hyper_flash_ops.c \
../drivers/fsl_cache.c \
../drivers/fsl_clock.c \
../drivers/fsl_common.c \
../drivers/fsl_dcp.c \
../drivers/fsl_dmamux.c \
../drivers/fsl_edma.c \
../drivers/fsl_flexram.c \
../drivers/fsl_flexram_allocate.c \
../drivers/fsl_flexspi.c \
../drivers/fsl_gpio.c \
../drivers/fsl_gpt.c \
../drivers/fsl_lpi2c.c \
../drivers/fsl_lpuart.c \
../drivers/fsl_pwm.c \
../drivers/fsl_sai.c \
../drivers/fsl_sai_edma.c \
../drivers/fsl_trng.c \
../drivers/fsl_usdhc.c \
../drivers/fsl_xbara.c 

OBJS += \
./drivers/flexspi_hyper_flash_ops.o \
./drivers/fsl_cache.o \
./drivers/fsl_clock.o \
./drivers/fsl_common.o \
./drivers/fsl_dcp.o \
./drivers/fsl_dmamux.o \
./drivers/fsl_edma.o \
./drivers/fsl_flexram.o \
./drivers/fsl_flexram_allocate.o \
./drivers/fsl_flexspi.o \
./drivers/fsl_gpio.o \
./drivers/fsl_gpt.o \
./drivers/fsl_lpi2c.o \
./drivers/fsl_lpuart.o \
./drivers/fsl_pwm.o \
./drivers/fsl_sai.o \
./drivers/fsl_sai_edma.o \
./drivers/fsl_trng.o \
./drivers/fsl_usdhc.o \
./drivers/fsl_xbara.o 

C_DEPS += \
./drivers/flexspi_hyper_flash_ops.d \
./drivers/fsl_cache.d \
./drivers/fsl_clock.d \
./drivers/fsl_common.d \
./drivers/fsl_dcp.d \
./drivers/fsl_dmamux.d \
./drivers/fsl_edma.d \
./drivers/fsl_flexram.d \
./drivers/fsl_flexram_allocate.d \
./drivers/fsl_flexspi.d \
./drivers/fsl_gpio.d \
./drivers/fsl_gpt.d \
./drivers/fsl_lpi2c.d \
./drivers/fsl_lpuart.d \
./drivers/fsl_pwm.d \
./drivers/fsl_sai.d \
./drivers/fsl_sai_edma.d \
./drivers/fsl_trng.d \
./drivers/fsl_usdhc.d \
./drivers/fsl_xbara.d 


# Each subdirectory must supply rules for building sources it contributes
drivers/%.o: ../drivers/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MIMXRT106SDVL6A -DCPU_MIMXRT106SDVL6A_cm7 -DSDK_DEBUGCONSOLE=1 -DPRINTF_FLOAT_ENABLE=0 -DSCANF_FLOAT_ENABLE=0 -DPRINTF_ADVANCED_ENABLE=0 -DSCANF_ADVANCED_ENABLE=0 -DXIP_EXTERNAL_FLASH=1 -DXIP_BOOT_HEADER_ENABLE=1 -DXIP_BOOT_HEADER_DCD_ENABLE=1 -DAPP_MAJ_VER=0x02 -DAPP_MIN_VER=0x08 -DAPP_BLD_VER=0x0000 -DBOARD_USE_VIRTUALCOM=1 -DDEBUG_CONSOLE_IO_USBCDC=1 -DDEBUG_CONSOLE_TRANSFER_NON_BLOCKING -DSERIAL_PORT_TYPE_USBCDC=1 -DUSB_CDC_SERIAL_MANAGER_RUN_NO_HOST=1 -DRELOCATE_VECTOR_TABLE=1 -DUSE_TFA=1 -DUSE_TFA9894_PUI=0 -DNO_DEBUG_MICS -DSLN_LOCAL2_IOT -D__SEMIHOST_HARDFAULT_DISABLE=1 -DSERIAL_MANAGER_NON_BLOCKING_MODE=1 -DUSB_STACK_FREERTOS_HEAP_SIZE=65536 -DUSB_STACK_FREERTOS -DARM_MATH_CM7 -DSDK_SAI_BASED_COMPONENT_USED=1 -DCR_INTEGER_PRINTF -DUSE_RTOS=1 -DFSL_RTOS_FREE_RTOS -DMBEDTLS_CONFIG_FILE='"mbedtls_config.h"' -DCONFIG_FLEXRAM_AT_STARTUP=1 -DSDK_I2C_BASED_COMPONENT_USED=1 -DUSB_DEVICE_CONFIG_CDC_ACM=1 -DSDIO_ENABLED -DSLN_AFE_LIB -DSLN_ASR_LIB -DSLN_DSP_TOOLBOX_LIB -DDEBUG_CONSOLE_RX_ENABLE=0 -DSERIAL_PORT_TYPE_UART=1 -DBOARD_USE_CODEC=1 -DCODEC_TFA9XXX_ENABLE -DWIFI_WICED -DSDK_DEBUGCONSOLE_UART -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\board" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\source" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\codec" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\drivers" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\component\i2c" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\component\serial_manager" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\component\lists" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\component\serial_manager\usb_cdc_adapter" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\port\arch" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\compat\posix\arpa" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\compat\posix\net" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\compat\posix" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\compat\posix\sys" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\compat\stdc" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\lwip" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\lwip\priv" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\lwip\prot" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\netif" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\netif\ppp" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include\netif\ppp\polarssl" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\utilities" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\platforms\MURATA_TYPE1DX" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\sdmmc\host" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\sdmmc\inc" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\audio" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\freertos\freertos_kernel\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\freertos\freertos_kernel\portable\GCC\ARM_CM4F" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\phy" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\device\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\device\source" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\component\osa" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\device\source\ehci" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\mbedtls\port\ksdk" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\audio\voice" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\device" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\component\uart" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\xip" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\CMSIS" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\device\class\cdc" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\device\class" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\codec\tfa9xxx" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\codec\tfa9xxx\vas_tfa_drv" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\internal\chips\4343W" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\RTOS\FreeRTOS\WWD" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\include\RTOS" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\include\network" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\include\platform" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\internal\bus_protocols\SDIO" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\internal\bus_protocols" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD\internal" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\WWD" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\network\LwIP\WWD" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\platform\MCU\LPC" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\platform\MCU" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\platform\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\WICED\platform" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\wiced\43xxx_Wi-Fi\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\sdmmc\osa" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\cjson\inc" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\usb\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\config_files" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\audio\audio_processing" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\local_voice" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\port" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\lwip\src\include" -I"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\mbedtls\include" -O2 -fno-common -g -Wall -fomit-frame-pointer  -c  -ffunction-sections  -fdata-sections  -ffreestanding  -fno-builtin -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


