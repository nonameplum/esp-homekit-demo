PROGRAM = garage_light

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/spiffs \
	extras/rboot-ota \
	extras/sntp \
	$(abspath ../../components/esp-8266/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit) \
	$(abspath ../../components/common/helpers) \
	$(abspath ../../../UDPlogger) \

ESPPORT=/dev/tty.SLAB_USBtoUART
#ESPPORT=/dev/cu.usbserial-AH0707RK
ESPBAUD=115200

FLASH_SIZE ?= 32
FLASH_MODE ?= dio
FLASH_SPEED ?= 40

INPUT_PIN ?= 5
RELAY_PIN ?= 4
LED_PIN ?= 2
SWITCH_PIN ?= 0

BUILD_DATETIME ?= "Build time: $(shell date "+%d-%m-%y %H:%M:%S")"
$(info $(BUILD_DATETIME))

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS 
EXTRA_CFLAGS += -DINPUT_PIN=$(INPUT_PIN) 
EXTRA_CFLAGS += -DRELAY_PIN=$(RELAY_PIN)
EXTRA_CFLAGS += -DLED_PIN=$(LED_PIN)
EXTRA_CFLAGS += -DSWITCH_PIN=$(SWITCH_PIN)
EXTRA_CFLAGS += -DconfigCHECK_FOR_STACK_OVERFLOW=1
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL
# EXTRA_CFLAGS += -DHOMEKIT_DEBUG
# EXTRA_CFLAGS += -DDEBUG_HELPER

EXTRA_CFLAGS += -DBUILD_DATETIME=\"$(BUILD_DATETIME)\"

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
