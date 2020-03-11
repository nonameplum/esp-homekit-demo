PROGRAM = DS18B20

EXTRA_COMPONENTS = \
	extras/onewire \
	extras/ds18b20 \
	extras/http-parser \
	extras/dhcpserver \
	extras/spiffs \
	extras/rboot-ota \
	extras/sntp \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit) \
	$(abspath ../../../UDPlogger) \
	$(abspath ../../helpers) \

# ESPPORT=/dev/tty.SLAB_USBtoUART
# ESPPORT=/dev/cu.usbserial-AH0707RK
ESPPORT=/dev/tty.usbserial-1420

ESPBAUD=115200

FLASH_SIZE ?= 32
FLASH_MODE ?= dio
FLASH_SPEED ?= 40

DATA_PIN ?= 5

BUILD_DATETIME ?= "$(shell date "+%Y-%d-%m %H:%M:%S")"
$(info "Build time: $(BUILD_DATETIME)")

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS 
EXTRA_CFLAGS += -DconfigCHECK_FOR_STACK_OVERFLOW=1

EXTRA_CFLAGS += -DDATA_PIN=$(DATA_PIN) 

EXTRA_CFLAGS += -DLWIP_NETIF_HOSTNAME=1
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL
# EXTRA_CFLAGS += -DDEBUG_HELPER
# EXTRA_CFLAGS += -DDEBUG_HELPER_UDP
# EXTRA_CFLAGS += -DHOMEKIT_DEBUG
# EXTRA_CFLAGS += -DHOMEKIT_LOG_DISABLE
# EXTRA_CFLAGS += -DqDebugLog
# EXTRA_CFLAGS += -DqLogIncoming
# EXTRA_CFLAGS += -DqLogAllTraffic

EXTRA_CFLAGS += -DBUILD_DATETIME=\"$(BUILD_DATETIME)\"

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
