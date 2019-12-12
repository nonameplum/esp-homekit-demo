PROGRAM = garage

EXTRA_COMPONENTS = \
	lwip \
	extras/http-parser \
	extras/dhcpserver \
	extras/sntp \
	$(abspath ../../components/esp-8266/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit) \
	$(abspath ../../components/esp-8266/led-status) \
	$(abspath ../../components/esp-8266/wifi_config) \
	$(abspath ../../../UDPlogger) \

#ESPPORT=/dev/tty.SLAB_USBtoUART
ESPPORT=/dev/cu.usbserial-AH0707RK
ESPBAUD=460800

REED_PIN ?= 4
LAMP_PIN ?= 5
RELAY_PIN ?= 12
LED_PIN ?= 13
BUTTON_PIN ?= 0

SONOFF=1
message=""
ifeq ($(SONOFF),1)
	FLASH_SIZE ?= 8
	FLASH_MODE ?= dout
	FLASH_SPEED ?= 40
	HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x7A000
	message = $(shell echo "sonoff setup")
else
	FLASH_SIZE ?= 32
	FLASH_MODE ?= dio
	FLASH_SPEED ?= 40
	
	REED_PIN ?= -4
	LAMP_PIN ?= 4
	RELAY_PIN ?= -3
	LED_PIN ?= 2
	BUTTON_PIN ?= -1
	message = $(shell echo "genric 4MB esp8266 setup")
endif
$(info $(message))

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DREED_PIN=$(REED_PIN) -DLAMP_PIN=$(LAMP_PIN) -DRELAY_PIN=$(RELAY_PIN) -DLED_PIN=$(LED_PIN) -DBUTTON_PIN=$(BUTTON_PIN) 
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL
EXTRA_CFLAGS += -DconfigCHECK_FOR_STACK_OVERFLOW=1
EXTRA_CFLAGS += -DGARAGE_DEBUG
#EXTRA_CFLAGS += -DHOMEKIT_DEBUG

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud ESPBAUD --elf $(PROGRAM_OUT)
