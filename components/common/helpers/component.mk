# Component makefile for plum

INC_DIRS += $(helpers_ROOT)

helpers_INC_DIR = $(helpers_ROOT)
helpers_SRC_DIR = $(helpers_ROOT)

$(eval $(call component_compile_rules,helpers))