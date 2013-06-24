LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
	sensortest_main.cpp
LOCAL_MODULE:= sensortest-ndk
LOCAL_MODULE_TAGS:=optional

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libgui libandroid_runtime libandroid

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/sensortest-ndk

