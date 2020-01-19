# Component makefile for helpers

ifndef udplogger_ROOT
    $(error Please include UDPlogger component prior to helpers)
endif

INC_DIRS += $(helpers_ROOT)

helpers_INC_DIR = $(helpers_ROOT)
helpers_SRC_DIR = $(helpers_ROOT)

$(eval $(call component_compile_rules,helpers))

# ifneq (,$(findstring DEBUG_HELPER_UDP,$(EXTRA_CFLAGS)))
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_TO_UDP
# EXTRA_CFLAGS += -DUDPLOG_PRINTF_ALSO_SERIAL
# else
# endif

# $(info $(EXTRA_CFLAGS))