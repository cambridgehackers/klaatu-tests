LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= evtest.c
LOCAL_MODULE:= evtest
LOCAL_MODULE_TAGS:=optional

# Without this you get a LOT of warning messages
LOCAL_CFLAGS += -Wno-override-init

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/evtest

