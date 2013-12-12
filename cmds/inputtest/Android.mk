LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	inputtest_main.cpp
LOCAL_MODULE:= inputtest
LOCAL_MODULE_TAGS:=optional

LOCAL_C_INCLUDES += $(ANDROID_BUILD_TOP)/frameworks/base/services
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libinput
SVERSION:=$(subst ., ,$(PLATFORM_VERSION))
SHORT_PLATFORM_VERSION=$(word 1,$(SVERSION))$(word 2,$(SVERSION))
LOCAL_CFLAGS += -DSHORT_PLATFORM_VERSION=$(SHORT_PLATFORM_VERSION)

ifeq ($(SHORT_PLATFORM_VERSION),44)
LOCAL_SHARED_LIBRARIES += libinputservice
endif

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/inputtest

