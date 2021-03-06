LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	phonetest_main.cpp
LOCAL_MODULE:= phonetest
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += external/klaatu-services/include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libklaatu_phone

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/phonetest

