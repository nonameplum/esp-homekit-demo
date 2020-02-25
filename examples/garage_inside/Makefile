PROGRAM = garage_inside

EXTRA_COMPONENTS = \
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

INPUT_PIN ?= 5  # D1
RELAY_PIN ?= 4  # D2
LED_PIN ?= 2    # D4
SWITCH_PIN ?= 0 # D3

BUILD_DATETIME ?= "$(shell date "+%Y-%d-%m %H:%M:%S")"
$(info "Build time: $(BUILD_DATETIME)")

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS 
EXTRA_CFLAGS += -DconfigCHECK_FOR_STACK_OVERFLOW=1

EXTRA_CFLAGS += -DINPUT_PIN=$(INPUT_PIN) 
EXTRA_CFLAGS += -DRELAY_PIN=$(RELAY_PIN)
EXTRA_CFLAGS += -DLED_PIN=$(LED_PIN)
EXTRA_CFLAGS += -DSWITCH_PIN=$(SWITCH_PIN)

EXTRA_CFLAGS += -DLWIP_NETIF_HOSTNAME=1
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL
EXTRA_CFLAGS += -DDEBUG_HELPER
# EXTRA_CFLAGS += -DDEBUG_HELPER_UDP
# EXTRA_CFLAGS += -DHOMEKIT_DEBUG
# EXTRA_CFLAGS += -DHOMEKIT_LOG_DISABLE
# EXTRA_CFLAGS += -DqDebugLog
# EXTRA_CFLAGS += -DqLogIncoming
# EXTRA_CFLAGS += -DqLogAllTraffic

EXTRA_CFLAGS += -DBUILD_DATETIME=\"$(BUILD_DATETIME)\"

EXTRA_CFLAGS += -DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1 -DLWIP_MDNS_SEARCH=1

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
