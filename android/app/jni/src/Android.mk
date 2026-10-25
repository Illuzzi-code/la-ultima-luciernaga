LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
GAME_PATH := ../../../..

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
                     $(LOCAL_PATH)/$(GAME_PATH)/src \
                     $(LOCAL_PATH)/$(GAME_PATH)/third_party

LOCAL_SRC_FILES := $(GAME_PATH)/src/main.cpp \
                   $(GAME_PATH)/src/gfx.cpp \
                   $(GAME_PATH)/src/audio.cpp \
                   $(GAME_PATH)/src/level.cpp

LOCAL_CPPFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
                  -Wno-missing-field-initializers

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
