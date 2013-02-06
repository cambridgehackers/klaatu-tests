LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    btcommon.cpp headsetBase.cpp service.cpp socket.cpp android_bluetooth_c.c
LOCAL_MODULE:= bluetest
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += external/klaatu-services/include
LOCAL_C_INCLUDES += external/dbus
LOCAL_C_INCLUDES += external/bluetooth/bluez/lib system/bluetooth/bluedroid/include

LOCAL_SHARED_LIBRARIES := libutils libbluedroid libdbus

ifeq ($(PLATFORM_VERSION),4.1.2)
ifeq ($(BOARD_HAVE_BLUETOOTH),true)
include $(BUILD_EXECUTABLE)
# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/bluetest
endif
endif
