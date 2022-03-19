################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include xip/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/platform/MCU/LPC/WWD/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/platform/MCU/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/network/LwIP/WWD/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/internal/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/WWD/internal/chips/4343W/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/WWD/internal/bus_protocols/SDIO/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/WWD/internal/bus_protocols/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/WWD/internal/subdir.mk
-include wiced/43xxx_Wi-Fi/WICED/RTOS/FreeRTOS/WWD/subdir.mk
-include utilities/subdir.mk
-include usb/phy/subdir.mk
-include usb/device/source/ehci/subdir.mk
-include usb/device/source/subdir.mk
-include usb/device/class/cdc/subdir.mk
-include usb/device/class/subdir.mk
-include startup/subdir.mk
-include source/subdir.mk
-include sdmmc/src/subdir.mk
-include sdmmc/osa/subdir.mk
-include sdmmc/host/subdir.mk
-include mbedtls/port/ksdk/subdir.mk
-include mbedtls/library/subdir.mk
-include lwip/src/netif/ppp/subdir.mk
-include lwip/src/netif/subdir.mk
-include lwip/src/core/ipv6/subdir.mk
-include lwip/src/core/ipv4/subdir.mk
-include lwip/src/core/subdir.mk
-include lwip/src/api/subdir.mk
-include lwip/port/subdir.mk
-include freertos/freertos_kernel/portable/MemMang/subdir.mk
-include freertos/freertos_kernel/portable/GCC/ARM_CM4F/subdir.mk
-include freertos/freertos_kernel/subdir.mk
-include drivers/subdir.mk
-include device/subdir.mk
-include component/uart/subdir.mk
-include component/serial_manager/usb_cdc_adapter/subdir.mk
-include component/serial_manager/subdir.mk
-include component/osa/subdir.mk
-include component/lists/subdir.mk
-include component/i2c/subdir.mk
-include codec/tfa9xxx/vas_tfa_drv/subdir.mk
-include codec/tfa9xxx/subdir.mk
-include codec/subdir.mk
-include cjson/src/subdir.mk
-include board/subdir.mk
-include audio/tones/subdir.mk
-include audio/demos/subdir.mk
-include audio/audio_processing/subdir.mk
-include audio/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: uhuademo.axf

# Tool invocations
uhuademo.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	arm-none-eabi-gcc -nostdlib -L"C:\Users\user\Documents\MCUXpressoIDE_11.3.1_5262\workspace\uhuademo\libs" -Xlinker --gc-sections -Xlinker -print-memory-usage -Xlinker -Map="uhuademo.map" -Xlinker --sort-section=alignment -Xlinker --cref -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -T sln_local2_iot_local_demo_Debug.ld -o "uhuademo.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) uhuademo.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size "uhuademo.axf" ; arm-none-eabi-objcopy -v -O binary "uhuademo.axf" "uhuademo.bin" ; # checksum -p MIMXRT106SxxxxA -d "uhuademo.bin"
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
