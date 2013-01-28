/*
** Copyright 2006, The Android Open Source Project
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

#ifndef ANDROID_BLUETOOTH_COMMON_H
#define ANDROID_BLUETOOTH_COMMON_H

// Set to 0 to enable verbose bluetooth logging
#define LOG_NDEBUG 1

#include "utils/Log.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/poll.h>
#include <dbus/dbus.h>
#include <bluetooth/bluetooth.h>
#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>

namespace android {
#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define BLUEZ_ERROR_IFC           "org.bluez.Error"

// It would be nicer to retrieve this from bluez using GetDefaultAdapter,
// but this is only possible when the adapter is up (and hcid is running).
// It is much easier just to hardcode bluetooth adapter to hci0
#define BLUETOOTH_ADAPTER_HCI_NUM 0
#define BLUEZ_ADAPTER_OBJECT_NAME BLUEZ_DBUS_BASE_PATH "/hci0"
#define BTADDR_SIZE 18   // size of BT address character array (including null)

// size of the dbus event loops pollfd structure, hopefully never to be grown
#define DEFAULT_INITIAL_POLLFD_COUNT 8

// ALOGE and free a D-Bus error
// Using #define so that __FUNCTION__ resolves usefully
#define LOG_AND_FREE_DBUS_ERROR_WITH_MSG(err, msg) \
    {   ALOGE("%s: D-Bus error in %s: %s (%s)", __FUNCTION__, \
        dbus_message_get_member((msg)), (err)->name, (err)->message); \
         dbus_error_free((err)); }
#define LOG_AND_FREE_DBUS_ERROR(err) \
    {   ALOGE("%s: D-Bus error: %s (%s)", __FUNCTION__, \
        (err)->name, (err)->message); \
        dbus_error_free((err)); }

dbus_bool_t dbus_func_async(int timeout_ms, void (*reply)(DBusMessage *, void *, void *), void *user, const char *path, const char *ifc, const char *func, int first_arg_type, ...); 
DBusMessage * dbus_func_args(const char *path, const char *ifc, const char *func, int first_arg_type, ...); 
int dbus_returns_int32(DBusMessage *reply);
int dbus_returns_uint32(DBusMessage *reply);
int dbus_returns_unixfd(DBusMessage *reply);

typedef KeyedVector<String8, String8> BTProperties;
int parse_properties(BTProperties& prop, DBusMessageIter *iter);
int parse_property_change(BTProperties& prop, DBusMessage *msg);
void append_dict_args(DBusMessage *reply, const char *first_key, ...);
void append_variant(DBusMessageIter *iter, int type, void *val);
int get_bdaddr(const char *str, bdaddr_t *ba);
void get_bdaddr_as_string(const bdaddr_t *ba, char *str);
bool debug_no_encrypt();

// Result codes from Bluez DBus calls
#define BOND_RESULT_ERROR                      -1
#define BOND_RESULT_SUCCESS                     0
#define BOND_RESULT_AUTH_FAILED                 1
#define BOND_RESULT_AUTH_REJECTED               2
#define BOND_RESULT_AUTH_CANCELED               3
#define BOND_RESULT_REMOTE_DEVICE_DOWN          4
#define BOND_RESULT_DISCOVERY_IN_PROGRESS       5
#define BOND_RESULT_AUTH_TIMEOUT                6
#define BOND_RESULT_REPEATED_ATTEMPTS           7

#define PAN_DISCONNECT_FAILED_NOT_CONNECTED  1000
#define PAN_CONNECT_FAILED_ALREADY_CONNECTED 1001
#define PAN_CONNECT_FAILED_ATTEMPT_FAILED    1002
#define PAN_OPERATION_GENERIC_FAILURE        1003
#define PAN_OPERATION_SUCCESS                1004

#define INPUT_DISCONNECT_FAILED_NOT_CONNECTED  5000
#define INPUT_CONNECT_FAILED_ALREADY_CONNECTED 5001
#define INPUT_CONNECT_FAILED_ATTEMPT_FAILED    5002
#define INPUT_OPERATION_GENERIC_FAILURE        5003
#define INPUT_OPERATION_SUCCESS                5004

#define HEALTH_OPERATION_SUCCESS               6000
#define HEALTH_OPERATION_ERROR                 6001
#define HEALTH_OPERATION_INVALID_ARGS          6002
#define HEALTH_OPERATION_GENERIC_FAILURE       6003
#define HEALTH_OPERATION_NOT_FOUND             6004
#define HEALTH_OPERATION_NOT_ALLOWED           6005

} /* namespace android */

#endif/*ANDROID_BLUETOOTH_COMMON_H*/
