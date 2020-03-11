PROGRAM = garage

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	extras/sntp \
	extras/rboot-ota \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit) \
	$(abspath ../../../UDPlogger) \
	$(abspath ../../helpers) \

# ESPPORT=/dev/tty.SLAB_USBtoUART
# ESPPORT=/dev/cu.usbserial-AH0707RK
ESPPORT=/dev/cu.usbserial-1410

ESPBAUD=115200

FLASH_SIZE ?= 32
FLASH_MODE ?= dio
FLASH_SPEED ?= 40

# BUTTON_PIN ?= 0 // not used
LED_PIN ?= 2 # XOR (LOW active)
RELAY_PIN ?= 4 # Relay (short connect to open/close garage door)
# INPUT_PIN ?= 5 # XOR (LOW active) // not used
LAMP_PIN ?= 12 # Output outside lamp
REED_PIN ?= 14 # Input Reed Switch

BUILD_DATETIME ?= "$(shell date "+%Y-%d-%m %H:%M:%S")"
$(info "Build time: $(BUILD_DATETIME)")

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS 
EXTRA_CFLAGS += -DconfigCHECK_FOR_STACK_OVERFLOW=1

EXTRA_CFLAGS += -DREED_PIN=$(REED_PIN)
EXTRA_CFLAGS += -DLAMP_PIN=$(LAMP_PIN)
EXTRA_CFLAGS += -DRELAY_PIN=$(RELAY_PIN)
EXTRA_CFLAGS += -DLED_PIN=$(LED_PIN)
# EXTRA_CFLAGS += -DBUTTON_PIN=$(BUTTON_PIN)
# EXTRA_CFLAGS += -DINPUT_PIN=$(INPUT_PIN)

EXTRA_CFLAGS += -DLWIP_NETIF_HOSTNAME=1
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL
EXTRA_CFLAGS += -DDEBUG_HELPER
# EXTRA_CFLAGS += -DDEBUG_HELPER_UDP
# EXTRA_CFLAGS += -DHOMEKIT_DEBUG
# EXTRA_CFLAGS += -DHOMEKIT_LOGS_OFF=1

EXTRA_CFLAGS += -DBUILD_DATETIME=\"$(BUILD_DATETIME)\"

EXTRA_CFLAGS += -DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1 -DLWIP_MDNS_SEARCH=1

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
