LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=         \
        cameracapture.cpp

LOCAL_SHARED_LIBRARIES := \
	libstagefright liblog libutils libbinder libstagefright_foundation \
	libcamera_client libgui libmedia

LOCAL_C_INCLUDES:= \
	frameworks/av/media/libstagefright \
	$(TOP)/frameworks/native/include/media/openmax

LOCAL_CFLAGS += -Wno-multichar

LOCAL_MODULE_TAGS := debug

LOCAL_MODULE:= cameracapture

include $(BUILD_EXECUTABLE)

################################################################################
