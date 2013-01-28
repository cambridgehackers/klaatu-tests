/*
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "BluetoothService.cpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dbus/dbus.h>
#include <bluedroid/bluetooth.h>
#include <cutils/properties.h>
#include "cutils/sockets.h"
//#include "android_runtime/AndroidRuntime.h"
#include "utils/Log.h"
#include "utils/misc.h"

#include "btcommon.h"

#undef ALOGE
#define ALOGE printf
#undef ALOGV
#define ALOGV printf

#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"
#define DBUS_INPUT_IFACE BLUEZ_DBUS_BASE_IFC ".Input"
#define DBUS_NETWORK_IFACE BLUEZ_DBUS_BASE_IFC ".Network"
#define DBUS_NETWORKSERVER_IFACE BLUEZ_DBUS_BASE_IFC ".NetworkServer"
#define DBUS_HEALTH_MANAGER_PATH "/org/bluez"
#define DBUS_HEALTH_MANAGER_IFACE BLUEZ_DBUS_BASE_IFC ".HealthManager"
#define DBUS_HEALTH_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".HealthDevice"
#define DBUS_HEALTH_CHANNEL_IFACE BLUEZ_DBUS_BASE_IFC ".HealthChannel"
static const char *agent_path = "/android/bluetooth/agent";
static const char *device_agent_path = "/android/bluetooth/remote_device_agent";

namespace android {

typedef struct {
    const char *str;
    int value;
} CHARMAPTYPE;

typedef struct {
    const char *group;
    const char *name;
    int value;
} SIGTABLETYPE;

#include "blueprop.h"

enum {BSIG_NOT_SIGNAL=0,
#define SIGDEF(A,B,C) C,
    SIGNITEMS
    METHITEMS
    };

#undef SIGDEF
#define SIGDEF(A,B,C) {(A), (B), (C)},
static SIGTABLETYPE sigtable[] = {
    SIGNITEMS
    {NULL, NULL, -1}};

static SIGTABLETYPE methtable[] = {
    METHITEMS
    {NULL, NULL, -1}};

static struct pollfd *pollData;
static DBusWatch **watchData;
static int pollMemberCount = 1; 
static int pollDataSize = DEFAULT_INITIAL_POLLFD_COUNT;
static int controlFdR;
static int controlFdW;
static DBusConnection *global_conn;
static const char *global_adapter;  // dbus object name of the local adapter
static void onConnectSinkResult(DBusMessage *msg, void *user, void *n);

static void dumpprop(BTProperties& prop, const char *name)
{
    for (size_t i = 0; i < prop.size(); ++i) {
        printf("prop %s: %s=%s\n", name, prop.keyAt(i).string(), prop.valueAt(i).string());
    }
//indexOfKey
}

static Vector<String8> getSinkPropertiesNative(String8 path) {
    DBusMessage *msg, *reply;
    DBusError err;
    dbus_error_init(&err);
Vector<String8> result;
BTProperties prop;
    int rc = -1;

    const char *c_path = path.string();
    reply = dbus_func_args_timeout(-1, c_path, "org.bluez.AudioSink", "GetProperties", DBUS_TYPE_INVALID);
    if (!reply && dbus_error_is_set(&err)) {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
        return result;
    } else if (!reply) {
        ALOGE("DBus reply is NULL in function %s", __FUNCTION__);
        return result;
    }
    DBusMessageIter iter;
    if (dbus_message_iter_init(reply, &iter))
        rc = parse_properties(prop, &iter); // sink_properties);
dumpprop(prop, "getsink");
    return result;
}

static bool connectSinkNative(String8 path) {
    const char *c_path = path.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback
    return dbus_func_args_async(-1, onConnectSinkResult, context_path, path.string(), "org.bluez.AudioSink", "Connect", DBUS_TYPE_INVALID); 
}

static bool disconnectSinkNative(String8 path) {
    return dbus_func_args_async(-1,NULL,NULL, path.string(), "org.bluez.AudioSink", "Disconnect", DBUS_TYPE_INVALID); 
}

static bool suspendSinkNative(String8 path) {
    return dbus_func_args_async(-1, NULL, NULL, path.string(), "org.bluez.audio.Sink", "Suspend", DBUS_TYPE_INVALID);
}

static bool resumeSinkNative(String8 path) {
    return dbus_func_args_async(-1, NULL, NULL, path.string(), "org.bluez.audio.Sink", "Resume", DBUS_TYPE_INVALID);
}

static bool avrcpVolumeUpNative(String8 path) {
    return dbus_func_args_async(-1, NULL, NULL, path.string(), "org.bluez.Control", "VolumeUp", DBUS_TYPE_INVALID);
}

static bool avrcpVolumeDownNative(String8 path) {
    return dbus_func_args_async(-1, NULL, NULL, path.string(), "org.bluez.Control", "VolumeDown", DBUS_TYPE_INVALID);
}

void onConnectSinkResult(DBusMessage *msg, void *user, void *n) {
    const char *path = (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    if (dbus_set_error_from_message(&err, msg)) {
        LOG_AND_FREE_DBUS_ERROR(&err);
    }
    ALOGV("... Device Path = %s\n", path); 
    free(user);
} 

static unsigned int unix_events_to_dbus_flags(short events) {
    return (events & DBUS_WATCH_READABLE ? POLLIN : 0) | (events & DBUS_WATCH_WRITABLE ? POLLOUT : 0) | (events & DBUS_WATCH_ERROR ? POLLERR : 0) | (events & DBUS_WATCH_HANGUP ? POLLHUP : 0);
}

static short dbus_flags_to_unix_events(unsigned int flags) {
    return (flags & POLLIN ? DBUS_WATCH_READABLE : 0) | (flags & POLLOUT ? DBUS_WATCH_WRITABLE : 0) | (flags & POLLERR ? DBUS_WATCH_ERROR : 0) | (flags & POLLHUP ? DBUS_WATCH_HANGUP : 0);
}

#define EVENT_LOOP_REFS 10
#define EVENT_LOOP_EXIT 1
#define EVENT_LOOP_ADD  2
#define EVENT_LOOP_REMOVE 3
#define EVENT_LOOP_WAKEUP 4

dbus_bool_t dbusAddWatch(DBusWatch *watch, void *data) {
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (dbus_watch_get_enabled(watch)) {
printf("[%s:%d] %d\n", __FUNCTION__, __LINE__, controlFdW);
        // note that we can't just send the watch and inspect it later
        // because we may get a removeWatch call before this data is reacted
        // to by our eventloop and remove this watch..  reading the add first
        // and then inspecting the recently deceased watch would be bad.
        char control = EVENT_LOOP_ADD;
        write(controlFdW, &control, sizeof(char)); 
        int fd = dbus_watch_get_fd(watch);
        write(controlFdW, &fd, sizeof(int)); 
        unsigned int flags = dbus_watch_get_flags(watch);
        write(controlFdW, &flags, sizeof(unsigned int)); 
        write(controlFdW, &watch, sizeof(DBusWatch*));
    }
    return true;
}

void dbusRemoveWatch(DBusWatch *watch, void *data) {
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    char control = EVENT_LOOP_REMOVE;
    write(controlFdW, &control, sizeof(char)); 
    int fd = dbus_watch_get_fd(watch);
    write(controlFdW, &fd, sizeof(int)); 
    unsigned int flags = dbus_watch_get_flags(watch);
    write(controlFdW, &flags, sizeof(unsigned int));
}

void dbusToggleWatch(DBusWatch *watch, void *data) {
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (dbus_watch_get_enabled(watch)) {
        dbusAddWatch(watch, data);
    } else {
        dbusRemoveWatch(watch, data);
    }
}

static void addmatch(void)
{
    DBusError err;
    const char **p = signames;
    while (*p) {
        dbus_bus_add_match(global_conn, *p, &err);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
            exit(1);
        } 
        p++;
    }
}
static void removematch(void)
{
    DBusError err;
    const char **p = signames;
    while (*p) {
        dbus_bus_remove_match(global_conn, *p, &err);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        p++;
    }
}

void dbusWakeup(void *data) {
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    char control = EVENT_LOOP_WAKEUP;
    write(controlFdW, &control, sizeof(char));
}

static const char * get_adapter_path(DBusConnection *conn) {
    DBusMessage *msg = NULL, *reply = NULL;
    DBusError err;
    const char *device_path = NULL;
    int attempt = 0;

    for (attempt = 0; attempt < 1000 && reply == NULL; attempt ++) {
        msg = dbus_message_new_method_call("org.bluez", "/", "org.bluez.Manager", "DefaultAdapter");
        dbus_message_append_args(msg, DBUS_TYPE_INVALID);
        dbus_error_init(&err);
        reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err); 
        if (!reply) {
            if (dbus_error_is_set(&err)) {
                if (dbus_error_has_name(&err, "org.freedesktop.DBus.Error.ServiceUnknown")) {
                    // bluetoothd is still down, retry
                    LOG_AND_FREE_DBUS_ERROR(&err);
                    usleep(10000);  // 10 ms
                    continue;
                } else {
                    // Some other error we weren't expecting
                    LOG_AND_FREE_DBUS_ERROR(&err);
                }
            }
            goto failed;
        }
    }
    if (attempt == 1000) {
        ALOGE("Time out while trying to get Adapter path, is bluetoothd up ?");
        goto failed;
    } 
    if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH, &device_path, DBUS_TYPE_INVALID) || !device_path){
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        goto failed;
    }
    dbus_message_unref(msg);
    return device_path;

failed:
    dbus_message_unref(msg);
    return NULL;
}

static int findsignal(SIGTABLETYPE *map, DBusMessage *msg)
{
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
        return BSIG_NOT_SIGNAL;
    while (map->group) {
        if (dbus_message_is_signal(msg, map->group, map->name))
            break;
        map++;
    }
    return map->value;
}
// Called by dbus during WaitForAndDispatchEventNative()
static DBusHandlerResult event_filter(DBusConnection *conn, DBusMessage *msg, void *data)
{
    DBusError err;
    DBusHandlerResult ret; 
    char *c_address;
    char *c_iface;
    uint16_t uuid; 
    bool exists = TRUE;
    DBusMessageIter iter;
    char *c_object_path;
    const char *c_channel_path;
    const char *c_path;
    BTProperties prop;
    int rc = -1;

    dbus_error_init(&err); 
    int sigvalue = findsignal(sigtable, msg);
    printf("%s: %d Received signal %s:%s from %s\n", __FUNCTION__, sigvalue, dbus_message_get_interface(msg), dbus_message_get_member(msg), dbus_message_get_path(msg)); 
    switch(sigvalue) {
    case BSIG_NOT_SIGNAL:
        ALOGV("%s: not interested (not a signal).", __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    case BSIG_AdapterDeviceFound:
        if (dbus_message_iter_init(msg, &iter)) {
            dbus_message_iter_get_basic(&iter, &c_address);
            if (dbus_message_iter_next(&iter))
                rc = parse_properties(prop, &iter); // remote_device_properties);
            int indexaddr = prop.indexOfKey(String8("Address"));
            if (indexaddr >= 0)
                printf("[%s:%d] Address %s\n", __FUNCTION__, __LINE__, prop.valueAt(indexaddr).string());
            dumpprop(prop, "adapterfound");
        }
        if (rc)
            goto failed;
        //c_address), str_array);
        break;
    case BSIG_AdapterDeviceDisappeared:
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &c_address, DBUS_TYPE_INVALID))
            goto failed;
        ALOGV("... address = %s", c_address);
        //c_address));
        break;
    case BSIG_AdapterDeviceCreated:
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_OBJECT_PATH, &c_object_path, DBUS_TYPE_INVALID))
            goto failed;
        ALOGV("... address = %s", c_object_path);
        //c_object_path));
        break;
    case BSIG_AdapterDeviceRemoved:
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_OBJECT_PATH, &c_object_path, DBUS_TYPE_INVALID))
            goto failed;
        ALOGV("... Object Path = %s", c_object_path);
        //c_object_path));
        break;
    case BSIG_AdapterPropertyChanged: {
        rc = parse_property_change(prop, msg); // adapter_properties);
        if (rc)
            goto failed;
        dumpprop(prop, "adapterchanged");
        int indexaddr = prop.indexOfKey(String8("Powered"));
        if (indexaddr >= 0)
            printf("[%s:%d] Powered %s\n", __FUNCTION__, __LINE__, prop.valueAt(indexaddr).string());
        /* Check if bluetoothd has (re)started, if so update the path. */
        //JAVA(method_onPropertyChanged, str_array);
        break;
        }
    case BSIG_DevicePropertyChanged: {
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        rc = parse_property_change(prop, msg); // remote_device_properties);
        if (rc)
            goto failed;
dumpprop(prop, "devchanged");
        const char *remote_device_path = dbus_message_get_path(msg);
        //remote_device_path), str_array);
        break;
        }
    case BSIG_DeviceDisconnectRequested: {
        const char *remote_device_path = dbus_message_get_path(msg);
        //remote_device_path));
        break;
        }
    case BSIG_InputDevicePropertyChanged:
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        rc = parse_property_change(prop, msg); // input_properties);
        if (rc)
            goto failed;
dumpprop(prop, "inpdevchanged");
        c_path = dbus_message_get_path(msg);
        //c_path), str_array);
        break;
    case BSIG_PanDevicePropertyChanged:
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        rc = parse_property_change(prop, msg); // pan_properties);
        if (rc)
            goto failed;
dumpprop(prop, "pandevchanged");
        c_path = dbus_message_get_path(msg);
        //c_path), str_array);
        break;
    case BSIG_NetworkDeviceDisconnected:
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &c_address, DBUS_TYPE_INVALID))
            goto failed;
        //c_address));
        break;
    case BSIG_NetworkDeviceConnected:
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &c_address, DBUS_TYPE_STRING, &c_iface, DBUS_TYPE_UINT16, &uuid, DBUS_TYPE_INVALID))
            goto failed;
        //c_address), String8(c_iface), uuid);
        break;
    case BSIG_HealthDeviceChannelConnected:
        c_path = dbus_message_get_path(msg);
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_OBJECT_PATH, &c_channel_path, DBUS_TYPE_INVALID))
            goto failed;
        //c_path), String8(c_channel_path), exists);
        break;
    case BSIG_HealthDeviceChannelDeleted:
        c_path = dbus_message_get_path(msg);
        exists = FALSE;
        if (!dbus_message_get_args(msg, &err, DBUS_TYPE_OBJECT_PATH, &c_channel_path, DBUS_TYPE_INVALID))
            goto failed;
        //c_path), String8(c_channel_path), exists);
        break;
    case BSIG_HealthDevicePropertyChanged:
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        rc = parse_property_change(prop, msg); // health_device_properties);
        if (rc)
            goto failed;
dumpprop(prop, "healdevchanged");
        c_path = dbus_message_get_path(msg);
        //c_path), str_array);
        break;
    case BSIG_AudioPropertyChanged:
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        rc = parse_property_change(prop, msg); // sink_properties);
        c_path = dbus_message_get_path(msg);
dumpprop(prop, "auddevchanged");
        break;
    default:
        goto failed;
    }
    return DBUS_HANDLER_RESULT_HANDLED;
failed:
    LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
printf("[%s:%d] end bad\n", __FUNCTION__, __LINE__);
    return DBUS_HANDLER_RESULT_HANDLED;
}
static void process_control(void)
{
                char data;
                while (recv(controlFdR, &data, sizeof(char), MSG_DONTWAIT) != -1) {
                    switch (data) {
                    case EVENT_LOOP_EXIT: {
                        dbus_connection_set_watch_functions(global_conn, NULL, NULL, NULL, NULL, NULL);
                        DBusMessage *msg, *reply;
                        DBusError err;
                        dbus_error_init(&err);
                        msg = dbus_message_new_method_call("org.bluez", global_adapter, "org.bluez.Adapter", "UnregisterAgent");
                        dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &agent_path, DBUS_TYPE_INVALID);
                        reply = dbus_connection_send_with_reply_and_block(global_conn, msg, -1, &err); 
                        if (!reply) {
                            if (dbus_error_is_set(&err)) {
                                LOG_AND_FREE_DBUS_ERROR(&err);
                                dbus_error_free(&err);
                            }
                        } else {
                            dbus_message_unref(reply);
                        }
                        dbus_message_unref(msg);
                        dbus_connection_flush(global_conn);
                        dbus_connection_unregister_object_path(global_conn, agent_path); 
                        removematch();
                        dbus_connection_remove_filter(global_conn, event_filter, NULL);
                        int fd = controlFdR;
                        controlFdR = 0;
                        close(fd);
                        return;
                    }
                    case EVENT_LOOP_ADD: {
                        DBusWatch *watch;
                        int newFD;
                        unsigned int flags; 
                        read(controlFdR, &newFD, sizeof(int));
                        read(controlFdR, &flags, sizeof(unsigned int));
                        read(controlFdR, &watch, sizeof(DBusWatch *));
                        short events = dbus_flags_to_unix_events(flags); 
                        for (int y = 0; y<pollMemberCount; y++) {
                            if ((pollData[y].fd == newFD) && (pollData[y].events == events)) {
                                ALOGV("DBusWatch duplicate add");
                                exit(1);
                            }
                        }
                        if (pollMemberCount == pollDataSize) {
                            ALOGV("Bluetooth EventLoop poll struct growing");
                            struct pollfd *temp = (struct pollfd *)malloc( sizeof(struct pollfd) * (pollMemberCount+1));
                            memcpy(temp, pollData, sizeof(struct pollfd) * pollMemberCount);
                            free(pollData);
                            pollData = temp;
                            DBusWatch **temp2 = (DBusWatch **)malloc(sizeof(DBusWatch *) * (pollMemberCount+1));
                            memcpy(temp2, watchData, sizeof(DBusWatch *) * pollMemberCount);
                            free(watchData);
                            watchData = temp2;
                            pollDataSize++;
                        }
                        pollData[pollMemberCount].fd = newFD;
                        pollData[pollMemberCount].revents = 0;
                        pollData[pollMemberCount].events = events;
                        watchData[pollMemberCount] = watch;
                        pollMemberCount++;
                        break;
                    }
                    case EVENT_LOOP_REMOVE: {
                        int removeFD;
                        unsigned int flags; 
                        read(controlFdR, &removeFD, sizeof(int));
                        read(controlFdR, &flags, sizeof(unsigned int));
                        short events = dbus_flags_to_unix_events(flags); 
                        for (int y = 0; y < pollMemberCount; y++) {
                            if ((pollData[y].fd == removeFD) && (pollData[y].events == events)) {
                                int newCount = --pollMemberCount;
                                // copy the last live member over this one
                                pollData[y].fd = pollData[newCount].fd;
                                pollData[y].events = pollData[newCount].events;
                                pollData[y].revents = pollData[newCount].revents;
                                watchData[y] = watchData[newCount];
                                goto switchover;
                            }
                        }
                        ALOGW("WatchRemove given with unknown watch");
                        break;
                    }
                    case EVENT_LOOP_WAKEUP: {
                        // noop
                        break;
                    }
                    default:
                        printf("[%s:%d]unknown\n", __FUNCTION__, __LINE__);
                    }
switchover:;
                }
}
static void *eventLoopMain(void)
{
    int sockvec[2];

printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    pollData = (struct pollfd *)calloc( DEFAULT_INITIAL_POLLFD_COUNT, sizeof(struct pollfd));
    watchData = (DBusWatch **)calloc( DEFAULT_INITIAL_POLLFD_COUNT, sizeof(DBusWatch *));
    if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockvec)) {
        ALOGE("Error getting BT control socket");
        exit(1);
    }
    controlFdR = sockvec[0];
    controlFdW = sockvec[1];
    pollData[0].fd = controlFdR;
    pollData[0].events = POLLIN; 
    dbus_connection_set_watch_functions(global_conn, dbusAddWatch, dbusRemoveWatch, dbusToggleWatch, NULL, NULL);
    dbus_connection_set_wakeup_main_function(global_conn, dbusWakeup, NULL, NULL); 
 
    while (1) {
        for (int i = 0; i < pollMemberCount; i++) {
            if (!pollData[i].revents) {
                continue;
            }
            if (pollData[i].fd == controlFdR) {
                process_control();
            } else {
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
                short events = pollData[i].revents;
                unsigned int flags = unix_events_to_dbus_flags(events);
                dbus_watch_handle(watchData[i], flags);
                pollData[i].revents = 0;
                // can only do one - it may have caused a 'remove'
                break;
            }
        }
        while (dbus_connection_dispatch(global_conn) == DBUS_DISPATCH_DATA_REMAINS) {
            } 
        poll(pollData, pollMemberCount, -1);
    }
}

static int findmethod(SIGTABLETYPE *map, DBusMessage *msg)
{
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return BSIG_NOT_SIGNAL;
    while (map->group) {
        if (dbus_message_is_method_call(msg, map->group, map->name))
            break;
        map++;
    }
    return map->value;
}
static DBusHandlerResult agent_event_filter(DBusConnection *conn, DBusMessage *msg, void *data)
{
    char *object_path;
    const char *uuid;
    uint32_t passkey;
    DBusMessage *reply;

    int methvalue = findsignal(sigtable, msg);
    printf("%s: Received method %s:%s\n", __FUNCTION__, dbus_message_get_interface(msg), dbus_message_get_member(msg)); 
    switch(methvalue) {
    case BSIG_NOT_SIGNAL:
        ALOGV("%s: not interested (not a method call).", __FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    case BMETH_AgentCancel:
        //JAVA(method_onAgentCancel);
        // reply
        reply = dbus_message_new_method_return(msg);
        dbus_connection_send(global_conn, reply, NULL);
        dbus_message_unref(reply);
        break;
    case BMETH_AgentAuthorize:
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_STRING, &uuid, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for Authorize() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        ALOGV("... object_path = %s", object_path);
        ALOGV("... uuid = %s", uuid); 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), String8(uuid), int(msg)); 
        break;
    case BMETH_AgentOutOfBandDataAvailable: {
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for OutOfBandData available() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        ALOGV("... object_path = %s", object_path); 
        bool available = false; //env->CallBooleanMethod(NULL, method_onAgentOutOfBandDataAvailable, String8(object_path)); 
        // reply
        if (available) {
            reply = dbus_message_new_method_return(msg);
            dbus_connection_send(global_conn, reply, NULL);
            dbus_message_unref(reply);
        } else {
            reply = dbus_message_new_error(msg, "org.bluez.Error.DoesNotExist", "OutofBand data not available");
            if (!reply) {
                ALOGE("%s: Cannot create message reply\n", __FUNCTION__);
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            }
            dbus_connection_send(global_conn, reply, NULL);
            dbus_message_unref(reply);
        }
        break;
        }
    case BMETH_RequestPinCode: {
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for RequestPinCode() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), int(msg));
        break;
        }
    case BMETH_RequestPasskey:
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for RequestPasskey() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), int(msg));
        break;
    case BMETH_RequestOobData:
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for RequestOobData() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), int(msg));
        break;
    case BMETH_DisplayPasskey:
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_UINT32, &passkey, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for RequestPasskey() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), passkey, int(msg));
        break;
    case BMETH_RequestPasskeyConfirmation:
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_UINT32, &passkey, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for RequestConfirmation() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), passkey, int(msg));
        break;
    case BMETH_RequestPairingConsent:
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID)) {
            ALOGE("%s: Invalid arguments for RequestPairingConsent() method", __FUNCTION__);
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        } 
        dbus_message_ref(msg);  // increment refcount because we pass to java
        //object_path), int(msg));
        break;
    case BMETH_Release:
        // reply
        reply = dbus_message_new_method_return(msg);
        dbus_connection_send(global_conn, reply, NULL);
        dbus_message_unref(reply);
        break;
    default:
        ALOGV("%s:%s is ignored", dbus_message_get_interface(msg), dbus_message_get_member(msg));
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; 
    } 
    return DBUS_HANDLER_RESULT_HANDLED;
}

static int lookupmap(CHARMAPTYPE *map, const char *name)
{
    while (map->str) {
        if (!strcmp(name, map->str))
            break;
        map++;
    }
    return map->value;
}

void onCreatePairedDeviceResult(DBusMessage *msg, void *user, void *n) {
    ALOGV("%s", __FUNCTION__); 
    const char *address = (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    String8 addr; 
    ALOGV("... address = %s", address); 
    int result = BOND_RESULT_SUCCESS;
    if (dbus_set_error_from_message(&err, msg)) {
        result = lookupmap(bondmap, err.name);
        switch(result) {
        case BOND_RESULT_AUTH_FAILED:
            // Pins did not match, or remote device did not respond to pin request in time
        case BOND_RESULT_AUTH_REJECTED:
            // We rejected pairing, or the remote side rejected pairing. This
            // happens if either side presses 'cancel' at the pairing dialog.
        case BOND_RESULT_AUTH_CANCELED: // Not sure if this happens
        case BOND_RESULT_REMOTE_DEVICE_DOWN: // Other device is not responding at all
        case BOND_RESULT_SUCCESS: // already bonded
        case BOND_RESULT_REPEATED_ATTEMPTS:
        case BOND_RESULT_AUTH_TIMEOUT:
            ALOGV("... error = %s (%s)\n", err.name, err.message);
            break;
        case BOND_RESULT_DISCOVERY_IN_PROGRESS:
            if (!strcmp(err.message, "Bonding in progress")) {
                ALOGV("... error = %s (%s)\n", err.name, err.message);
                result = BOND_RESULT_SUCCESS;
                break;
            } else if (!strcmp(err.message, "Discover in progress")) {
                ALOGV("... error = %s (%s)\n", err.name, err.message);
                break;
            }
        default:
            ALOGE("%s: D-Bus error: %s (%s)\n", __FUNCTION__, err.name, err.message);
            result = BOND_RESULT_ERROR;
            break;
        }
    } 
    dbus_error_free(&err);
    free(user);
}

#define CREATE_DEVICE_ALREADY_EXISTS 1
#define CREATE_DEVICE_SUCCESS 0
#define CREATE_DEVICE_FAILED -1

void onCreateDeviceResult(DBusMessage *msg, void *user, void *n) {
    ALOGV("%s", __FUNCTION__); 
    const char *address= (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    ALOGV("... Address = %s", address); 
    int result = CREATE_DEVICE_SUCCESS;
    if (dbus_set_error_from_message(&err, msg)) {
        if (dbus_error_has_name(&err, "org.bluez.Error.AlreadyExists")) {
            result = CREATE_DEVICE_ALREADY_EXISTS;
        } else {
            result = CREATE_DEVICE_FAILED;
        }
        LOG_AND_FREE_DBUS_ERROR(&err);
    }
    free(user);
}

void onGetDeviceServiceChannelResult(DBusMessage *msg, void *user, void *n) {
    const char *address = (const char *) user;
    DBusError err;
    dbus_error_init(&err);
    int channel = -2;
    ALOGV("... address = %s", address); 
    if (dbus_set_error_from_message(&err, msg) ||
        !dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &channel, DBUS_TYPE_INVALID)) {
        ALOGE("%s: D-Bus error: %s (%s)\n", __FUNCTION__, err.name, err.message);
        dbus_error_free(&err);
    }
done:
    free(user);
}

void onInputDeviceConnectionResult(DBusMessage *msg, void *user, void *n) {
    const char *path = (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    int result = INPUT_OPERATION_SUCCESS;
    if (dbus_set_error_from_message(&err, msg)) {
        result = lookupmap(inputconnectmap, err.name);
        switch(result) {
        case INPUT_DISCONNECT_FAILED_NOT_CONNECTED:
            if (strcmp(err.message, "Transport endpoint is not connected"))
                result = INPUT_OPERATION_GENERIC_FAILURE;
            break;
        }
        LOG_AND_FREE_DBUS_ERROR(&err);
    } 
    ALOGV("... Device Path = %s, result = %d", path, result);
    free(user);
}

void onPanDeviceConnectionResult(DBusMessage *msg, void *user, void *n) {
    const char *path = (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    int result = PAN_OPERATION_SUCCESS;
    if (dbus_set_error_from_message(&err, msg)) {
        result = lookupmap(panconnectmap, err.name);
        switch(result) {
        case PAN_DISCONNECT_FAILED_NOT_CONNECTED:
            // TODO():This is flaky, need to change Bluez to add new error codes
            if (!strcmp(err.message, "Device already connected")) {
                result = PAN_CONNECT_FAILED_ALREADY_CONNECTED;
            } else if (strcmp(err.message, "Device not connected")) {
                result = PAN_OPERATION_GENERIC_FAILURE;
            }
            break;
        }
        LOG_AND_FREE_DBUS_ERROR(&err);
    }
    ALOGV("... Pan Device Path = %s, result = %d", path, result);
    free(user);
}

void onHealthDeviceConnectionResult(DBusMessage *msg, void *user, void *n) {
    DBusError err;
    dbus_error_init(&err);
    int result = HEALTH_OPERATION_SUCCESS;
    if (dbus_set_error_from_message(&err, msg)) {
        result = lookupmap(healthmap, err.name);
        LOG_AND_FREE_DBUS_ERROR(&err);
    }

    int code = *(int *) user;
    ALOGV("... Health Device Code = %d, result = %d", code, result);
    free(user);
}

// This function is called when the adapter is enabled.
static bool setupNativeDataNative() {
    // Register agent for remote devices.
    static const DBusObjectPathVTable agent_vtable = { NULL, agent_event_filter, NULL, NULL, NULL, NULL }; 
    if (!dbus_connection_register_object_path(global_conn, device_agent_path, &agent_vtable, NULL)) {
        ALOGE("%s: Can't register object path %s for remote device agent!", __FUNCTION__, device_agent_path);
        return FALSE;
    }
    return TRUE;
}

static bool tearDownNativeDataNative() {
    dbus_connection_unregister_object_path (global_conn, device_agent_path);
    return TRUE;
}

static bool stopDiscoveryNative() {
    DBusError err;
    const char *name;
    bool ret = FALSE; 
    dbus_error_init(&err); 
    DBusMessage *msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, global_adapter, DBUS_ADAPTER_IFACE, "StopDiscovery");
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(global_conn, msg, -1, &err);
    if (dbus_error_is_set(&err)) {
        if(strncmp(err.name, BLUEZ_DBUS_BASE_IFC ".Error.NotAuthorized", strlen(BLUEZ_DBUS_BASE_IFC ".Error.NotAuthorized")) == 0) {
            // hcid sends this if there is no active discovery to cancel
            ALOGV("%s: There was no active discovery to cancel", __FUNCTION__);
            dbus_error_free(&err);
        } else {
            LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
        }
        goto done;
    } 
    ret = TRUE;
done:
    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return ret;
}

static char * readAdapterOutOfBandDataNative() {
    DBusError err;
    char *hash, *randomizer;
    char * byteArray = NULL;
    int hash_len, r_len;
    DBusMessage *reply = dbus_func_args(global_adapter, DBUS_ADAPTER_IFACE, "ReadLocalOutOfBandData", DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    if (dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &hash, &hash_len, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &randomizer, &r_len, DBUS_TYPE_INVALID)) {
        if (hash_len == 16 && r_len == 16) {
            byteArray =NULL;// env->NewByteArray(32);
            if (byteArray) {
                //env->SetByteArrayRegion(byteArray, 0, 16, hash);
                //env->SetByteArrayRegion(byteArray, 16, 16, randomizer);
            }
        } else {
            ALOGE("readAdapterOutOfBandDataNative: Hash len = %d, R len = %d", hash_len, r_len);
        }
    } else {
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
    dbus_message_unref(reply);
    return byteArray;
}

static bool createPairedDeviceNative(String8 address, int timeout_ms) {
    const char *c_address = address.string();
    ALOGV("... address = %s", c_address);
    char *context_address = (char *)calloc(BTADDR_SIZE, sizeof(char));
    const char *capabilities = "DisplayYesNo";
    strlcpy(context_address, c_address, BTADDR_SIZE);  // for callback
    return dbus_func_args_async((int)timeout_ms, onCreatePairedDeviceResult, context_address, global_adapter, DBUS_ADAPTER_IFACE, "CreatePairedDevice", DBUS_TYPE_STRING, &c_address, DBUS_TYPE_OBJECT_PATH, &device_agent_path, DBUS_TYPE_STRING, &capabilities, DBUS_TYPE_INVALID);
}

static bool createPairedDeviceOutOfBandNative(String8 address, int timeout_ms) {
    const char *c_address = address.string();
    ALOGV("... address = %s", c_address);
    char *context_address = (char *)calloc(BTADDR_SIZE, sizeof(char));
    const char *capabilities = "DisplayYesNo";
    strlcpy(context_address, c_address, BTADDR_SIZE);  // for callback
    return dbus_func_args_async((int)timeout_ms, onCreatePairedDeviceResult, context_address, global_adapter, DBUS_ADAPTER_IFACE, "CreatePairedDeviceOutOfBand", DBUS_TYPE_STRING, &c_address, DBUS_TYPE_OBJECT_PATH, &device_agent_path, DBUS_TYPE_STRING, &capabilities, DBUS_TYPE_INVALID);
}

static int getDeviceServiceChannelNative(String8 path, String8 pattern, int attr_id) {
    const char *c_pattern = pattern.string();
    const char *c_path = path.string();
    ALOGV("... pattern = %s", c_pattern);
    ALOGV("... attr_id = %#X", attr_id);
    DBusMessage *reply = dbus_func_args(c_path, DBUS_DEVICE_IFACE, "GetServiceAttributeValue", DBUS_TYPE_STRING, &c_pattern, DBUS_TYPE_UINT16, &attr_id, DBUS_TYPE_INVALID);
    return reply ? dbus_returns_int32(reply) : -1;
}

static bool cancelDeviceCreationNative(String8 address) {
    bool result = FALSE;
    const char *c_address = address.string();
    DBusError err;
    dbus_error_init(&err);
    ALOGV("... address = %s", c_address);
    DBusMessage *reply = dbus_func_args_timeout(-1, global_adapter, DBUS_ADAPTER_IFACE, "CancelDeviceCreation", DBUS_TYPE_STRING, &c_address, DBUS_TYPE_INVALID);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        } else
            ALOGE("DBus reply is NULL in function %s", __FUNCTION__);
        return FALSE;
    } else {
        result = TRUE;
    }
    dbus_message_unref(reply);
    return FALSE;
}

static bool removeDeviceNative(String8 object_path) {
    const char *c_object_path = object_path.string();
    return dbus_func_args_async(-1, NULL, NULL, global_adapter, DBUS_ADAPTER_IFACE, "RemoveDevice", DBUS_TYPE_OBJECT_PATH, &c_object_path, DBUS_TYPE_INVALID);
}

static bool setPairingConfirmationNative(String8 address, bool confirm, int nativeData) {
    DBusMessage *msg = (DBusMessage *)nativeData;
    DBusMessage *reply;
    if (confirm) {
        reply = dbus_message_new_method_return(msg);
    } else {
        reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected", "User rejected confirmation");
    } 
    if (!reply) {
        ALOGE("%s: Cannot create message reply to RequestPasskeyConfirmation or" "RequestPairingConsent to D-Bus\n", __FUNCTION__);
        dbus_message_unref(msg);
        return FALSE;
    } 
    dbus_connection_send(global_conn, reply, NULL);
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return TRUE;
}

static bool setPasskeyNative(String8 address, int passkey, int nativeData) {
    DBusMessage *msg = (DBusMessage *)nativeData;
    DBusMessage *reply = dbus_message_new_method_return(msg);
    dbus_message_append_args(reply, DBUS_TYPE_UINT32, (uint32_t *)&passkey, DBUS_TYPE_INVALID); 
    dbus_connection_send(global_conn, reply, NULL);
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return TRUE;
}

static bool setRemoteOutOfBandDataNative(String8 address, char * hash, char * randomizer, int nativeData) {
    DBusMessage *msg = (DBusMessage *)nativeData;
    DBusMessage *reply = dbus_message_new_method_return(msg);
    char *h_ptr =NULL; // env->GetByteArrayElements(hash, NULL);
    char *r_ptr =NULL; // env->GetByteArrayElements(randomizer, NULL);
    dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &h_ptr, 16, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &r_ptr, 16, DBUS_TYPE_INVALID); 
    dbus_connection_send(global_conn, reply, NULL);
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return TRUE;
}

static bool setAuthorizationNative(String8 address, bool val, int nativeData) {
    DBusMessage *msg = (DBusMessage *)nativeData;
    DBusMessage *reply;
    if (val) {
        reply = dbus_message_new_method_return(msg);
    } else {
        reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected", "Authorization rejected");
    }
    dbus_connection_send(global_conn, reply, NULL);
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return TRUE;
}

static bool setPinNative(String8 address, String8 pin, int nativeData) {
    DBusMessage *msg = (DBusMessage *)nativeData;
    DBusMessage *reply = dbus_message_new_method_return(msg);
    const char *c_pin = pin.string();
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &c_pin, DBUS_TYPE_INVALID); 
    dbus_connection_send(global_conn, reply, NULL);
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return TRUE;
}

static bool cancelPairingUserInputNative(String8 address, int nativeData) {
    DBusMessage *msg = (DBusMessage *)nativeData;
    DBusMessage *reply = dbus_message_new_error(msg, "org.bluez.Error.Canceled", "Pairing User Input was canceled");
    if (!reply) {
        ALOGE("%s: Cannot create message reply to return cancelUserInput to" "D-BUS\n", __FUNCTION__);
        dbus_message_unref(msg);
        return FALSE;
    } 
    dbus_connection_send(global_conn, reply, NULL);
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return TRUE;
}

static Vector<String8> getDevicePropertiesNative(String8 path)
{
    DBusMessageIter iter;
    Vector<String8> str_array;
    DBusMessage *msg, *reply;
    DBusError err;
BTProperties prop;
    int rc = -1;
    dbus_error_init(&err); 
    const char *c_path = path.string();
    reply = dbus_func_args_timeout(-1, c_path, DBUS_DEVICE_IFACE, "GetProperties", DBUS_TYPE_INVALID);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        } else
            ALOGE("DBus reply is NULL in function %s", __FUNCTION__);
        return str_array;
    }
    //env->PushLocalFrame(PROPERTIES_NREFS); 
    if (dbus_message_iter_init(reply, &iter))
       rc = parse_properties(prop, &iter); // remote_device_properties);
dumpprop(prop, "devprop");
    dbus_message_unref(reply);
    return str_array;
}

static Vector<String8> getAdapterPropertiesNative()
{
    Vector<String8> str_array;
    DBusMessage *msg, *reply;
    DBusError err;
    dbus_error_init(&err); 
BTProperties prop;
    int rc = -1;
    reply = dbus_func_args_timeout(-1, global_adapter, DBUS_ADAPTER_IFACE, "GetProperties", DBUS_TYPE_INVALID);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        } else
            ALOGE("DBus reply is NULL in function %s", __FUNCTION__);
        return str_array;
    }
    //env->PushLocalFrame(PROPERTIES_NREFS); 
    DBusMessageIter iter;
    if (dbus_message_iter_init(reply, &iter))
        rc = parse_properties(prop, &iter); // adapter_properties);
dumpprop(prop, "adapprop");
    dbus_message_unref(reply);
    return str_array;
}

static bool setAdapterPropertyNative(String8 key, void *value, int type) {
    DBusMessage *msg;
    DBusMessageIter iter;
    dbus_bool_t reply = FALSE;
    const char *c_key = key.string();
    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, global_adapter, DBUS_ADAPTER_IFACE, "SetProperty");
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &c_key, DBUS_TYPE_INVALID);
    dbus_message_iter_init_append(msg, &iter);
    append_variant(&iter, type, value); 
    // Asynchronous call - the callbacks come via propertyChange
    reply = dbus_connection_send_with_reply(global_conn, msg, NULL, -1);
    dbus_message_unref(msg); 
    return reply ? TRUE : FALSE; 
}

static bool setAdapterPropertyStringNative(String8 key, String8 value) {
    const char *c_value = value.string();
    return setAdapterPropertyNative( key, (void *)&c_value, DBUS_TYPE_STRING);
}

static bool setAdapterPropertyIntegerNative(String8 key, int value) {
    return setAdapterPropertyNative( key, (void *)&value, DBUS_TYPE_UINT32);
}

static bool setAdapterPropertyBooleanNative(String8 key, int value) {
    return setAdapterPropertyNative( key, (void *)&value, DBUS_TYPE_BOOLEAN);
}

static bool setDevicePropertyNative(String8 path, String8 key, void *value, int type) {
    DBusMessage *msg;
    DBusMessageIter iter;
    dbus_bool_t reply = FALSE; 
    const char *c_key = key.string();
    const char *c_path = path.string();
    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, c_path, DBUS_DEVICE_IFACE, "SetProperty");
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &c_key, DBUS_TYPE_INVALID);
    dbus_message_iter_init_append(msg, &iter);
    append_variant(&iter, type, value); 
    // Asynchronous call - the callbacks come via Device propertyChange
    reply = dbus_connection_send_with_reply(global_conn, msg, NULL, -1);
    dbus_message_unref(msg); 
    return reply ? TRUE : FALSE;
}

static bool setDevicePropertyBooleanNative(String8 path, String8 key, int value) {
    return setDevicePropertyNative( path, key, (void *)&value, DBUS_TYPE_BOOLEAN);
}

static bool setDevicePropertyStringNative(String8 path, String8 key, String8 value) {
    const char *c_value = value.string();
    return setDevicePropertyNative( path, key, (void *)&c_value, DBUS_TYPE_STRING);
}

static bool createDeviceNative(String8 address) {
    const char *c_address = address.string();
    ALOGV("... address = %s", c_address);
    char *context_address = (char *)calloc(BTADDR_SIZE, sizeof(char));
    strlcpy(context_address, c_address, BTADDR_SIZE);  // for callback

    return dbus_func_args_async(-1, onCreateDeviceResult, context_address, global_adapter, DBUS_ADAPTER_IFACE, "CreateDevice", DBUS_TYPE_STRING, &c_address, DBUS_TYPE_INVALID);
}

void onDiscoverServicesResult(DBusMessage *msg, void *user, void *n) {
    const char *path = (const char *)user;
    DBusError err;
    dbus_error_init(&err);
    ALOGV("... Device Path = %s", path); 
    bool result = TRUE;
    if (dbus_set_error_from_message(&err, msg)) {
        LOG_AND_FREE_DBUS_ERROR(&err);
        result = FALSE;
    }
    //String8 jPath = String8(path);
    //JAVA(method_onDiscoverServicesResult, jPath, result);
    free(user);
}

static bool discoverServicesNative(String8 path, String8 pattern) {
    const char *c_path = path.string();
    const char *c_pattern = pattern.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback

    ALOGV("... Object Path = %s", c_path);
    ALOGV("... Pattern = %s, strlen = %d", c_pattern, strlen(c_pattern));

    return dbus_func_args_async(-1, onDiscoverServicesResult, context_path, c_path, DBUS_DEVICE_IFACE, "DiscoverServices", DBUS_TYPE_STRING, &c_pattern, DBUS_TYPE_INVALID);
}

static int * extract_handles(DBusMessage *reply) {
    int *handles;
    int * handleArray = NULL;
    int len;

    DBusError err;
    dbus_error_init(&err); 
    if (dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &handles, &len, DBUS_TYPE_INVALID)) {
        //handleArray = env->NewIntArray(len);
        if (handleArray) {
            //env->SetIntArrayRegion(handleArray, 0, len, handles);
        } else {
            ALOGE("Null array in extract_handles");
        }
    } else {
        LOG_AND_FREE_DBUS_ERROR(&err);
    }
    return handleArray;
}

static int * addReservedServiceRecordsNative(int * uuids) {
    int* svc_classes =NULL; // env->GetIntArrayElements(uuids, NULL);
    if (!svc_classes) return NULL;
    int len =0; // env->GetArrayLength(uuids);
    DBusMessage *reply = dbus_func_args(global_adapter, DBUS_ADAPTER_IFACE, "AddReservedServiceRecords", DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &svc_classes, len, DBUS_TYPE_INVALID);
    return reply ? extract_handles(reply) : NULL;
}

static bool removeReservedServiceRecordsNative(int * handles) {
    int *values =NULL; // env->GetIntArrayElements(handles, NULL);
    DBusMessage *msg = NULL;
    if (values == NULL) return FALSE; 
    int len =0; // env->GetArrayLength(handles); 
    DBusMessage *reply = dbus_func_args(global_adapter, DBUS_ADAPTER_IFACE, "RemoveReservedServiceRecords", DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &values, len, DBUS_TYPE_INVALID);
    return reply ? TRUE : FALSE;
}

static int addRfcommServiceRecordNative(String8 name, long long uuidMsb, long long uuidLsb, short channel) {
    const char *c_name = name.string();
    printf("... name = %s uuid1 = %llX, uuid2 = %llX, channel = %d\n", c_name, uuidMsb, uuidLsb, channel);
    DBusMessage *reply = dbus_func_args(global_adapter, DBUS_ADAPTER_IFACE, "AddRfcommServiceRecord", DBUS_TYPE_STRING, &c_name, DBUS_TYPE_UINT64, &uuidMsb, DBUS_TYPE_UINT64, &uuidLsb, DBUS_TYPE_UINT16, &channel, DBUS_TYPE_INVALID);
    return reply ? dbus_returns_uint32(reply) : -1;
}

static bool removeServiceRecordNative(int handle) {
    ALOGV("... handle = %X", handle);
    DBusMessage *reply = dbus_func_args(global_adapter, DBUS_ADAPTER_IFACE, "RemoveServiceRecord", DBUS_TYPE_UINT32, &handle, DBUS_TYPE_INVALID);
    return reply ? TRUE : FALSE;
}

static bool setLinkTimeoutNative(String8 object_path, int num_slots) {
    const char *c_object_path = object_path.string();
    DBusMessage *reply = dbus_func_args(global_adapter, DBUS_ADAPTER_IFACE, "SetLinkTimeout", DBUS_TYPE_OBJECT_PATH, &c_object_path, DBUS_TYPE_UINT32, &num_slots, DBUS_TYPE_INVALID);
    return reply ? TRUE : FALSE;
}

static bool connectInputDeviceNative(String8 path) {
    const char *c_path = path.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback 
    return dbus_func_args_async(-1, onInputDeviceConnectionResult, context_path, c_path, DBUS_INPUT_IFACE, "Connect", DBUS_TYPE_INVALID); 
}

static bool disconnectInputDeviceNative(String8 path) {
    const char *c_path = path.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback 
    return dbus_func_args_async(-1, onInputDeviceConnectionResult, context_path, c_path, DBUS_INPUT_IFACE, "Disconnect", DBUS_TYPE_INVALID); 
}

static bool setBluetoothTetheringNative(bool value, String8 src_role, String8 bridge) {
    DBusMessage *reply;
    const char *c_role = src_role.string();
    const char *c_bridge = bridge.string();
    if (value) {
        ALOGE("setBluetoothTetheringNative true");
        reply = dbus_func_args(global_adapter, DBUS_NETWORKSERVER_IFACE, "Register", DBUS_TYPE_STRING, &c_role, DBUS_TYPE_STRING, &c_bridge, DBUS_TYPE_INVALID);
    } else {
        ALOGE("setBluetoothTetheringNative false");
        reply = dbus_func_args(global_adapter, DBUS_NETWORKSERVER_IFACE, "Unregister", DBUS_TYPE_STRING, &c_role, DBUS_TYPE_INVALID);
    }
    return reply ? TRUE : FALSE;
}

static bool connectPanDeviceNative(String8 path, String8 dstRole) {
    const char *c_path = path.string();
    const char *dst = dstRole.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback 
    return dbus_func_args_async(-1,onPanDeviceConnectionResult, context_path, c_path, DBUS_NETWORK_IFACE, "Connect", DBUS_TYPE_STRING, &dst, DBUS_TYPE_INVALID); 
}

static bool disconnectPanDeviceNative(String8 path) {
    const char *c_path = path.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback 
    return dbus_func_args_async(-1,onPanDeviceConnectionResult, context_path, c_path, DBUS_NETWORK_IFACE, "Disconnect", DBUS_TYPE_INVALID); 
}

static bool disconnectPanServerDeviceNative(String8 path, String8 address, String8 iface) {
    const char *c_address = address.string();
    const char *c_path = path.string();
    const char *c_iface = iface.string();
    int len = path.length() + 1;
    char *context_path = (char *)calloc(len, sizeof(char));
    strlcpy(context_path, c_path, len);  // for callback 
    return dbus_func_args_async(-1, onPanDeviceConnectionResult, context_path, global_adapter, DBUS_NETWORKSERVER_IFACE, "DisconnectDevice", DBUS_TYPE_STRING, &c_address, DBUS_TYPE_STRING, &c_iface, DBUS_TYPE_INVALID); 
}

static String8 registerHealthApplicationNative(int dataType, String8 role, String8 name, String8 channelType) {
    String8 path;
    const char *c_role = role.string();
    const char *c_name = name.string();
    const char *c_channel_type = channelType.string();
    char *c_path;
    DBusMessage *msg, *reply;
    DBusError err;
    dbus_error_init(&err); 
    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, DBUS_HEALTH_MANAGER_PATH, DBUS_HEALTH_MANAGER_IFACE, "CreateApplication"); 
    /* append arguments */
    append_dict_args(msg, "DataType", DBUS_TYPE_UINT16, &dataType, "Role", DBUS_TYPE_STRING, &c_role, "Description", DBUS_TYPE_STRING, &c_name, "ChannelType", DBUS_TYPE_STRING, &c_channel_type, DBUS_TYPE_INVALID); 
    /* Make the call. */
    reply = dbus_connection_send_with_reply_and_block(global_conn, msg, -1, &err); 
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH, &c_path, DBUS_TYPE_INVALID)) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            }
        } else {
           path = String8(c_path);
        }
        dbus_message_unref(reply);
    }
    return path;
}

static String8 registerSinkHealthApplicationNative(int dataType, String8 role, String8 name) {
    String8 path;
    const char *c_role = role.string();
    const char *c_name = name.string();
    char *c_path; 
    DBusMessage *msg, *reply;
    DBusError err;
    dbus_error_init(&err); 
    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, DBUS_HEALTH_MANAGER_PATH, DBUS_HEALTH_MANAGER_IFACE, "CreateApplication"); 
    /* append arguments */
    append_dict_args(msg, "DataType", DBUS_TYPE_UINT16, &dataType, "Role", DBUS_TYPE_STRING, &c_role, "Description", DBUS_TYPE_STRING, &c_name, DBUS_TYPE_INVALID); 
    /* Make the call. */
    reply = dbus_connection_send_with_reply_and_block(global_conn, msg, -1, &err); 
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH, &c_path, DBUS_TYPE_INVALID)) {
            if (dbus_error_is_set(&err)) {
                LOG_AND_FREE_DBUS_ERROR(&err);
            }
        } else {
            path = String8(c_path);
        }
        dbus_message_unref(reply);
    }
    return path;
}

static bool unregisterHealthApplicationNative(String8 path) {
    bool result = FALSE;
    const char *c_path = path.string();
    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_func_args_timeout(-1, DBUS_HEALTH_MANAGER_PATH, DBUS_HEALTH_MANAGER_IFACE, "DestroyApplication", DBUS_TYPE_OBJECT_PATH, &c_path, DBUS_TYPE_INVALID); 
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        result = TRUE;
    }
    return result;
}

static bool createChannelNative(String8 devicePath, String8 appPath, String8 config, int code) {
    const char *c_device_path = devicePath.string();
    const char *c_app_path = appPath.string();
    const char *c_config = config.string();
    int *data = (int *) malloc(sizeof(int));
    *data = code;
    return dbus_func_args_async(-1, onHealthDeviceConnectionResult, data, c_device_path, DBUS_HEALTH_DEVICE_IFACE, "CreateChannel", DBUS_TYPE_OBJECT_PATH, &c_app_path, DBUS_TYPE_STRING, &c_config, DBUS_TYPE_INVALID); 
}

static bool destroyChannelNative(String8 devicePath, String8 channelPath, int code) {
    const char *c_device_path = devicePath.string();
    const char *c_channel_path = channelPath.string();
    int *data = (int *) malloc(sizeof(int));
    *data = code;
    return dbus_func_args_async(-1, onHealthDeviceConnectionResult, data, c_device_path, DBUS_HEALTH_DEVICE_IFACE, "DestroyChannel", DBUS_TYPE_OBJECT_PATH, &c_channel_path, DBUS_TYPE_INVALID); 
}

static String8 getMainChannelNative(String8 devicePath) {
    const char *c_device_path = devicePath.string();
BTProperties prop;
    DBusError err;
    dbus_error_init(&err); 
    int rc = -1;
    DBusMessage *reply = dbus_func_args(c_device_path, DBUS_HEALTH_DEVICE_IFACE, "GetProperties", DBUS_TYPE_INVALID);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        DBusMessageIter iter;
        Vector<String8> str_array;
        if (dbus_message_iter_init(reply, &iter))
            rc = parse_properties(prop, &iter); // health_device_properties);
dumpprop(prop, "mainchan");
        dbus_message_unref(reply);
        String8 path = str_array[1]; 
        return path;
    }
    return String8("");
}

static String8 getChannelApplicationNative(String8 channelPath) {
    const char *c_channel_path = channelPath.string();
BTProperties prop;
    int rc = -1;
    DBusError err;
    dbus_error_init(&err); 
    DBusMessage *reply = dbus_func_args(c_channel_path, DBUS_HEALTH_CHANNEL_IFACE, "GetProperties", DBUS_TYPE_INVALID);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
    } else {
        DBusMessageIter iter;
        Vector<String8> str_array;
        if (dbus_message_iter_init(reply, &iter))
            rc = parse_properties(prop, &iter); // health_channel_properties);
dumpprop(prop, "chanapp");
        dbus_message_unref(reply); 
        int len = str_array.size(); 
        String8 name, path;
        const char *c_name; 
        for (int i = 0; i < len; i+=2) {
            name = str_array[i];
            c_name = name.string();
            if (!strcmp(c_name, "Application")) {
                path = str_array[i+1];
                return path;
            }
        }
    }
    return String8("");
}

static bool releaseChannelFdNative(String8 channelPath) {
    const char *c_channel_path = channelPath.string();
    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_func_args(c_channel_path, DBUS_HEALTH_CHANNEL_IFACE, "Release", DBUS_TYPE_INVALID);
    return reply ? TRUE : FALSE;
}

static int getChannelFdNative(String8 channelPath) {
    const char *c_channel_path = channelPath.string();
    int32_t fd;
    DBusError err;
    dbus_error_init(&err);

    DBusMessage *reply = dbus_func_args(c_channel_path, DBUS_HEALTH_CHANNEL_IFACE, "Acquire", DBUS_TYPE_INVALID);
    if (!reply) {
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        return -1;
    }
    fd = dbus_returns_unixfd(reply);
    if (fd == -1) return -1;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
       ALOGE("Can't get flags with fcntl(): %s (%d)", strerror(errno), errno);
       releaseChannelFdNative( channelPath);
       close(fd);
       return -1;
    } 
    flags &= ~O_NONBLOCK;
    int status = fcntl(fd, F_SETFL, flags);
    if (status < 0) {
       ALOGE("Can't set flags with fcntl(): %s (%d)", strerror(errno), errno);
       releaseChannelFdNative( channelPath);
       close(fd);
       return -1;
    } 
    // Create FileDescriptor object
    int fileDesc = 0; //jniCreateFileDescriptor(fd);
    if (fileDesc < 0) {
        // FileDescriptor constructor has thrown an exception
        releaseChannelFdNative( channelPath);
        close(fd);
        return -1;
    } 
#if 0
    // Wrap it in a ParcelFileDescriptor
    jobject parcelFileDesc = newParcelFileDescriptor(fileDesc);
    if (parcelFileDesc == NULL) {
        // ParcelFileDescriptor constructor has thrown an exception
        releaseChannelFdNative( channelPath);
        close(fd);
        return NULL;
    } 
    return parcelFileDesc;
#endif
    return fileDesc;
}

static const DBusObjectPathVTable agent_vtable = { NULL, agent_event_filter, NULL, NULL, NULL, NULL }; 
static int register_agent(const char * capabilities)
{
    DBusMessage *msg, *reply;
    DBusError err;
    dbus_bool_t oob = TRUE;

    if (!dbus_connection_register_object_path(global_conn, agent_path, &agent_vtable, NULL)) {
        ALOGE("%s: Can't register object path %s for agent!", __FUNCTION__, agent_path);
        return -1;
    }
    global_adapter = get_adapter_path(global_conn);
    if (global_adapter == NULL) {
        return -1;
    }
    msg = dbus_message_new_method_call("org.bluez", global_adapter, "org.bluez.Adapter", "RegisterAgent");
    dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &agent_path, DBUS_TYPE_STRING, &capabilities, DBUS_TYPE_INVALID); 
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(global_conn, msg, -1, &err);
    dbus_message_unref(msg); 
    if (!reply) {
        ALOGE("%s: Can't register agent!", __FUNCTION__);
        if (dbus_error_is_set(&err)) {
            LOG_AND_FREE_DBUS_ERROR(&err);
        }
        return -1;
    } 
    dbus_message_unref(reply);
    dbus_connection_flush(global_conn); 
    return 0;
}

void initme(void)
{
printf("[%s:%d] start\n", __FUNCTION__, __LINE__);
    DBusError err;
    dbus_error_init(&err);
    dbus_threads_init_default();
    global_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
printf("[%s:%d] global_conn %p\n", __FUNCTION__, __LINE__, global_conn);
    if (dbus_error_is_set(&err)) {
        printf("Could not get onto the system bus: %s\n", err.message);
        dbus_error_free(&err);
        exit(1);
    }
    dbus_connection_set_exit_on_disconnect(global_conn, FALSE);
    printf ("enabled %d\n", bt_is_enabled());
    if (!bt_is_enabled())
        bt_enable();
    dbus_error_init(&err);
    if (register_agent("DisplayYesNo") < 0) {
        dbus_connection_unregister_object_path (global_conn, agent_path);
printf("[%s:%d] registration failed\n", __FUNCTION__, __LINE__);
        exit(1);
    }
    // Add a filter for all incoming messages
    if (!dbus_connection_add_filter(global_conn, event_filter, NULL, NULL)){
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        exit(1);
    }
    // Set which messages will be processed by this dbus connection
    addmatch();
printf ("pathname %s\n", global_adapter);

    DBusMessage *msg = NULL;
    DBusMessage *reply = NULL;
    const char *name;
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    dbus_error_init(&err); 
    msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, global_adapter, DBUS_ADAPTER_IFACE, "StartDiscovery"); 
    reply = dbus_connection_send_with_reply_and_block(global_conn, msg, -1, &err);
    if (dbus_error_is_set(&err)) {
         LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
         exit(1);
    } 
    if (reply) dbus_message_unref(reply);
    if (msg) dbus_message_unref(msg);
    //bt_disable();
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    eventLoopMain();
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    dbus_connection_close(global_conn);
}

} /* namespace android */

int main(int argc, char *argv[])
{
    printf("[%s:%d] start\n", __FUNCTION__, __LINE__);
    android::initme();
    printf("[%s:%d] end\n", __FUNCTION__, __LINE__);
    return 0;
}
