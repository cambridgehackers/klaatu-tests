LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	sensor.cpp

LOCAL_CFLAGS:= -I.. 

LOCAL_C_INCLUDES += external/klaatu-services/include
LOCAL_SHARED_LIBRARIES := \
	libcutils libutils libui libgui libhardware libklaatu_sensors

LOCAL_MODULE:= test-sensor

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
