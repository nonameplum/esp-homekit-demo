PROGRAM = led

EXTRA_COMPONENTS = \
	extras/http-parser \
	$(abspath ../../components/esp-8266/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit)

# ESPPORT=/dev/tty.SLAB_USBtoUART
ESPPORT=/dev/cu.usbserial-AH0707RK

FLASH_SIZE ?= 32
FLASH_MODE ?= dio
FLASH_SPEED ?= 40

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)