LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	wifitest_main.cpp
LOCAL_MODULE:= wifitest
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libwifi

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/wifitest

