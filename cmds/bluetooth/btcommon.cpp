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

#define LOG_TAG "bluetooth_common.cpp"

#include "btcommon.h"
#include "utils/Log.h"
#include "utils/misc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <dbus/dbus.h>

namespace android {
extern DBusConnection *global_conn;

typedef struct {
    void (*user_cb)(DBusMessage *, void *, void *);
    void *user;
} dbus_async_call_t;

void dbus_func_args_async_callback(DBusPendingCall *call, void *data) { 
    dbus_async_call_t *req = (dbus_async_call_t *)data;
    DBusMessage *msg; 
    /* This is guaranteed to be non-NULL, because this function is called only
       when once the remote method invokation returns. */
    msg = dbus_pending_call_steal_reply(call); 
    if (msg) {
        if (req->user_cb) {
            // The user may not deref the message object.
            req->user_cb(msg, req->user, NULL);
        }
        dbus_message_unref(msg);
    }

    //dbus_message_unref(req->method);
    dbus_pending_call_cancel(call);
    dbus_pending_call_unref(call);
    free(req);
}

static dbus_bool_t dbus_func_args_async_valist(int timeout_ms, void (*user_cb)(DBusMessage *, void *, void*), void *user, const char *path, const char *ifc, const char *func, int first_arg_type, va_list args)
{
    dbus_bool_t reply = FALSE;

    /* Compose the command */
    DBusMessage *msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, path, ifc, func); 
    if (msg == NULL) {
        printf("Could not allocate D-Bus message object!");
        goto done;
    }

    /* append arguments */
    if (!dbus_message_append_args_valist(msg, first_arg_type, args)) {
        printf("Could not append argument to method call!");
        goto done;
    }

    {
    /* Make the call. */
    dbus_async_call_t *pending = (dbus_async_call_t *)malloc(sizeof(dbus_async_call_t));
    if (pending) {
        DBusPendingCall *call; 
        pending->user_cb = user_cb;
        pending->user = user;
        //pending->method = msg; 
        reply = dbus_connection_send_with_reply(global_conn, msg, &call, timeout_ms);
        if (reply == TRUE) {
            dbus_pending_call_set_notify(call, dbus_func_args_async_callback, pending, NULL);
        }
    }
    }

done:
    if (msg) dbus_message_unref(msg);
    return reply;
}

dbus_bool_t dbus_func_args_async(int timeout_ms, void (*reply)(DBusMessage *, void *, void*), void *user, const char *path, const char *ifc, const char *func, int first_arg_type, ...) {
    dbus_bool_t ret;
    va_list lst;
    va_start(lst, first_arg_type);

    ret = dbus_func_args_async_valist(timeout_ms, reply, user, path, ifc, func, first_arg_type, lst);
    va_end(lst);
    return ret;
}

DBusMessage * dbus_func_args_timeout_valist(int timeout_ms, DBusError *err, const char *path, const char *ifc, const char *func, int first_arg_type, va_list args)
{ 
    DBusMessage *reply = NULL;
    if (err) {
        err = (DBusError*)malloc(sizeof(DBusError));
        dbus_error_init(err);
    }

    /* Compose the command */
    DBusMessage *msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, path, ifc, func); 
    if (msg == NULL) {
        printf("Could not allocate D-Bus message object!");
        goto done;
    }

    /* append arguments */
    if (!dbus_message_append_args_valist(msg, first_arg_type, args)) {
        printf("Could not append argument to method call!");
        goto done;
    }

    /* Make the call. */
    reply = dbus_connection_send_with_reply_and_block(global_conn, msg, timeout_ms, err);
    if (err && dbus_error_is_set(err)) {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(err, msg);
    }

done:
    if (err) {
        free(err);
    }
    if (msg) dbus_message_unref(msg);
    return reply;
}

DBusMessage * dbus_func_args_timeout(int timeout_ms, const char *path, const char *ifc, const char *func, int first_arg_type, ...) {
    DBusMessage *ret;
    va_list lst;
    va_start(lst, first_arg_type);
    ret = dbus_func_args_timeout_valist(timeout_ms, NULL, path, ifc, func, first_arg_type, lst);
    va_end(lst);
    return ret;
}

DBusMessage * dbus_func_args(const char *path, const char *ifc, const char *func, int first_arg_type, ...) {
    DBusMessage *ret;
    va_list lst;
    va_start(lst, first_arg_type);
    ret = dbus_func_args_timeout_valist(-1, NULL, path, ifc, func, first_arg_type, lst);
    va_end(lst);
    return ret;
}

DBusMessage * dbus_func_args_error(DBusError *err, const char *path, const char *ifc, const char *func, int first_arg_type, ...) {
    DBusMessage *ret;
    va_list lst;
    va_start(lst, first_arg_type);
    ret = dbus_func_args_timeout_valist(-1, err, path, ifc, func, first_arg_type, lst);
    va_end(lst);
    return ret;
}

int dbus_returns_unixfd(DBusMessage *reply) {

    DBusError err;
    int ret = -1; 
    dbus_error_init(&err);
    if (!dbus_message_get_args(reply, &err, DBUS_TYPE_UNIX_FD, &ret, DBUS_TYPE_INVALID)) {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }
    dbus_message_unref(reply);
    return ret;
}

int dbus_returns_int32(DBusMessage *reply) { 
    DBusError err;
    int ret = -1; 
    dbus_error_init(&err);
    if (!dbus_message_get_args(reply, &err, DBUS_TYPE_INT32, &ret, DBUS_TYPE_INVALID)) {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }
    dbus_message_unref(reply);
    return ret;
}

int dbus_returns_uint32(DBusMessage *reply) { 
    DBusError err;
    int ret = -1; 
    dbus_error_init(&err);
    if (!dbus_message_get_args(reply, &err, DBUS_TYPE_UINT32, &ret, DBUS_TYPE_INVALID)) {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }
    dbus_message_unref(reply);
    return ret;
}

String8 dbus_returns_string(DBusMessage *reply) { 
    DBusError err;
    String8 ret;
    const char *name; 
    dbus_error_init(&err);
    if (dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
        //ret = env->NewStringUTF(name);
    } else {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }
    dbus_message_unref(reply); 
    return ret;
}

static bool dbus_returns_boolean(DBusMessage *reply) {
    DBusError err;
    bool ret = 0;
    dbus_bool_t val = FALSE; 
    dbus_error_init(&err); 
    /* Check the return value. */
    if (dbus_message_get_args(reply, &err, DBUS_TYPE_BOOLEAN, &val, DBUS_TYPE_INVALID)) {
        ret = val;
    } else {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }
    dbus_message_unref(reply);
    return ret;
}
#if 0
static Vector<String8> dbus_returns_array_of_object_path(DBusMessage *reply) { 
    DBusError err;
    char **list;
    int i, len;
    Vector<String8> strArray;

    dbus_error_init(&err);
    if (dbus_message_get_args (reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &list, &len, DBUS_TYPE_INVALID)) {
        String8 classNameStr; 

        //for (i = 0; i < len; i++)
            //set_object_array_element(strArray, list[i], i);
    } else {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }

    dbus_message_unref(reply);
    return strArray;
}

Vector<String8> dbus_returns_array_of_strings(DBusMessage *reply) { 
    Vector<String8> result;
    DBusError err;
    char **list;
    int i, len;
    dbus_error_init(&err);
    if (dbus_message_get_args (reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &list, &len, DBUS_TYPE_INVALID)) {
        String8 classNameStr; 
        printf("%s: there are %d elements in string array!", __FUNCTION__, len); 
        for (i = 0; i < len; i++)
            result.push(String8(list[i]));
    } else {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }
    dbus_message_unref(reply);
    return result;
}

jbyteArray dbus_returns_array_of_bytes(DBusMessage *reply) { 
    DBusError err;
    int i, len;
    jbyte *list;
    jbyteArray byteArray = NULL;

    dbus_error_init(&err);
    if (dbus_message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &list, &len, DBUS_TYPE_INVALID)) {
        printf("%s: there are %d elements in byte array!", __FUNCTION__, len);
        //byteArray = env->NewByteArray(len);
        //if (byteArray)
            //env->SetByteArrayRegion(byteArray, 0, len, list); 
    } else {
        LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
    }

    dbus_message_unref(reply);
    return byteArray;
}
#endif 
void append_variant(DBusMessageIter *iter, int type, void *val)
{
    DBusMessageIter value_iter;
    char var_type[2] = { type, '\0'};
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, var_type, &value_iter);
    dbus_message_iter_append_basic(&value_iter, type, val);
    dbus_message_iter_close_container(iter, &value_iter);
}

static void dict_append_entry(DBusMessageIter *dict, const char *key, int type, void *val)
{
    DBusMessageIter dict_entry;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry); 
    dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);
    append_variant(&dict_entry, type, val);
    dbus_message_iter_close_container(dict, &dict_entry);
}

static void append_dict_valist(DBusMessageIter *iterator, const char *first_key, va_list var_args)
{
    DBusMessageIter dict;
    int val_type;
    const char *val_key;
    void *val; 
    dbus_message_iter_open_container(iterator, DBUS_TYPE_ARRAY, DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict); 
    val_key = first_key;
    while (val_key) {
            val_type = va_arg(var_args, int);
            val = va_arg(var_args, void *);
            dict_append_entry(&dict, val_key, val_type, val);
            val_key = va_arg(var_args, char *);
    } 
    dbus_message_iter_close_container(iterator, &dict);
}

void append_dict_args(DBusMessage *reply, const char *first_key, ...)
{
    DBusMessageIter iter;
    va_list var_args; 
    dbus_message_iter_init_append(reply, &iter); 
    va_start(var_args, first_key);
    append_dict_valist(&iter, first_key, var_args);
    va_end(var_args);
}

static int get_property(DBusMessageIter iter, Properties *properties, int *prop_index, char * *value)
{
    DBusMessageIter prop_val, array_val_iter;
    char *property = NULL;
    uint32_t array_type;
    char *str_val;
    int itemindex = 0, j, int_val;

    *value = NULL;
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
        return -1;
    dbus_message_iter_get_basic(&iter, &property);
    if (!dbus_message_iter_next(&iter)
     || dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
        return -1;
printf("[%s:%d] property '%s'\n", __FUNCTION__, __LINE__, property);
    while (properties[itemindex].name) {
        if (!strncmp(property, properties[itemindex].name, strlen(property)))
            break;
        itemindex++;
    }
    *prop_index = itemindex;
    if (!properties[itemindex].name)
        return -1;

    dbus_message_iter_recurse(&iter, &prop_val);
    int type = properties[itemindex].type;
    if (dbus_message_iter_get_arg_type(&prop_val) != type) {
        printf("Property type mismatch in get_property: %d, expected:%d, index:%d", dbus_message_iter_get_arg_type(&prop_val), type, *prop_index);
        return -1;
    }

    char inttemp[32];
    switch(type) {
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
        dbus_message_iter_get_basic(&prop_val, value);
        break;
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_BOOLEAN:
        dbus_message_iter_get_basic(&prop_val, &int_val);
        sprintf(inttemp, "%d", int_val);
        *value = strdup(inttemp);
        break;
    case DBUS_TYPE_ARRAY:
        dbus_message_iter_recurse(&prop_val, &array_val_iter);
        array_type = dbus_message_iter_get_arg_type(&array_val_iter);
        //value->array_val = NULL;
        if (array_type == DBUS_TYPE_OBJECT_PATH ||
            array_type == DBUS_TYPE_STRING){
            j = 0;
            do {
               j ++;
            } while(dbus_message_iter_next(&array_val_iter));
            dbus_message_iter_recurse(&prop_val, &array_val_iter);
            // Allocate  an array of char *
            //*len = j;
            char **tmp = (char **)malloc(sizeof(char *) * j);
            if (!tmp)
                return -1;
            j = 0;
            do {
               dbus_message_iter_get_basic(&array_val_iter, &tmp[j]);
               j ++;
            } while(dbus_message_iter_next(&array_val_iter));
            //value->array_val = tmp;
        }
        break;
    default:
        return -1;
    }
    return 0;
}

static void create_prop_array(Vector<String8> strArray, Properties *property, char * *value, int len, int *array_index ) {
    char **prop_val = NULL;
    char buf[32] = {'\0'}, buf1[32] = {'\0'};
    int i; 
    const char *name = property->name;
    int prop_type = property->type; 
    //set_object_array_element(strArray, name, *array_index);
    *array_index += 1; 
    if (prop_type == DBUS_TYPE_UINT32 || prop_type == DBUS_TYPE_INT16) {
        //sprintf(buf, "%d", value->int_val);
        //set_object_array_element(strArray, buf, *array_index);
        *array_index += 1;
    } else if (prop_type == DBUS_TYPE_BOOLEAN) {
        //sprintf(buf, "%s", value->int_val ? "true" : "false"); 
        //set_object_array_element(strArray, buf, *array_index);
        *array_index += 1;
    } else if (prop_type == DBUS_TYPE_ARRAY) {
        // Write the length first
        sprintf(buf1, "%d", len);
        //set_object_array_element(strArray, buf1, *array_index);
        *array_index += 1; 
        //prop_val = value->array_val;
        for (i = 0; i < len; i++) {
            //set_object_array_element(strArray, prop_val[i], *array_index);
            *array_index += 1;
        }
    } else {
        //set_object_array_element(strArray, (const char *) *value, *array_index);
        *array_index += 1;
    }
}

int parse_properties(BTProperties& prop, DBusMessageIter *iter, Properties *properties)
{
    Vector<String8> result;
    DBusMessageIter dict_entry, dict;
    char * value;
    int i, array_index = 0;
    int prop_type = DBUS_TYPE_INVALID, type;
    int t, j;

printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    DBusError err;
    dbus_error_init(&err);
    if(dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
        goto failure;
    dbus_message_iter_recurse(iter, &dict);
    do {
        int prop_index = -1;
        if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_DICT_ENTRY)
            goto failure;
        dbus_message_iter_recurse(&dict, &dict_entry); 
        if (get_property(dict_entry, properties, &prop_index, &value))
            goto failure;
        char *p = value;
        if (!p)
            p = (char *)"(none)";
        result.push(String8(p));
        //values[prop_index].value = value;
    } while(dbus_message_iter_next(&dict)); 
    return 0;
failure:
    if (dbus_error_is_set(&err))
        LOG_AND_FREE_DBUS_ERROR(&err);
    return 1;
}

int parse_property_change(BTProperties& prop, DBusMessage *msg, Properties *properties) {
    DBusMessageIter iter;
    DBusError err;
    Vector<String8> strArray;
    int prop_index = -1;
    int array_index = 0, size = 0;
    char * value;

    dbus_error_init(&err);
    if (!dbus_message_iter_init(msg, &iter))
        goto failure;

    if (!get_property(iter, properties, &prop_index, &value)) {
        //create_prop_array(strArray, &properties[prop_index], &value, &array_index); 
        //if (properties[prop_index].type == DBUS_TYPE_ARRAY && value.array_val != NULL)
             //free(value.array_val); 
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
        return 0;
    }
failure:
printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
    return 1;
}

int get_bdaddr(const char *str, bdaddr_t *ba) {
    char *d = ((char *)ba) + 5, *endp;
    int i;
    for(i = 0; i < 6; i++) {
        *d-- = strtol(str, &endp, 16);
        if (*endp != ':' && i != 5) {
            memset(ba, 0, sizeof(bdaddr_t));
            return -1;
        }
        str = endp + 1;
    }
    return 0;
}

void get_bdaddr_as_string(const bdaddr_t *ba, char *str) {
    const uint8_t *b = (const uint8_t *)ba;
    sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", b[5], b[4], b[3], b[2], b[1], b[0]);
}

bool debug_no_encrypt() {
    return false;
#if 0
    char value[PROPERTY_VALUE_MAX] = "";

    property_get("debug.bt.no_encrypt", value, "");
    if (!strncmp("true", value, PROPERTY_VALUE_MAX) ||
        !strncmp("1", value, PROPERTY_VALUE_MAX)) {
        printf("mandatory bluetooth encryption disabled");
        return true;
    } else {
        return false;
    }
#endif
}
String8 bt_urlencode(const char *arg, int len)
{
   String8 retval;
// 'a' 'z'
// 'A' 'Z'
// ~ ! * ( ) '
   return retval;
}

const char *bt_urldecode(String8 arg, int *len)
{
   char *retval = NULL;
   return retval;
}

} /* namespace android */
