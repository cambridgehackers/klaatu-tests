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

#define LOG_TAG "BluetoothSocket.cpp"
//#define LOG_TAG "BluetoothAudioGateway.cpp"

#include "btcommon.h"
#include "android_bluetooth_c.h"
//#include "android_runtime/AndroidRuntime.h"
#include "utils/Log.h"
#include "utils/misc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <ctype.h>
#include <sys/poll.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sco.h>

#include "cutils/abort_socket.h"

#include <sys/ioctl.h>
#include <bluetooth/l2cap.h>

#define TYPE_AS_STR(t) ((t) == TYPE_RFCOMM ? "RFCOMM" : ((t) == TYPE_SCO ? "SCO" : "L2CAP"))
namespace android {
typedef struct {
    int hcidev;
    int hf_ag_rfcomm_channel;
    int hs_ag_rfcomm_channel;
    int hf_ag_rfcomm_sock;
    int hs_ag_rfcomm_sock;
} native_data_t;

static void initializeNativeDataNative() {
    native_data_t *nat = (native_data_t *)calloc(1, sizeof(native_data_t));
    nat->hcidev = BLUETOOTH_ADAPTER_HCI_NUM; 
    //env->SetIntField(object, field_mNativeData, (int)nat);
    //nat->hf_ag_rfcomm_channel = env->GetIntField(object, field_mHandsfreeAgRfcommChannel);
    //nat->hs_ag_rfcomm_channel = env->GetIntField(object, field_mHeadsetAgRfcommChannel);
    ALOGV("HF RFCOMM channel = %d.", nat->hf_ag_rfcomm_channel);
    ALOGV("HS RFCOMM channel = %d.", nat->hs_ag_rfcomm_channel); 
    /* Set the default values of these to -1. */
    //env->SetIntField(object, field_mConnectingHeadsetRfcommChannel, -1);
    //env->SetIntField(object, field_mConnectingHandsfreeRfcommChannel, -1); 
    nat->hf_ag_rfcomm_sock = -1;
    nat->hs_ag_rfcomm_sock = -1;
}

static int set_nb(int sk, bool nb) {
    int flags = fcntl(sk, F_GETFL);
    if (flags < 0) {
        ALOGE("Can't get socket flags with fcntl(): %s (%d)", strerror(errno), errno);
        close(sk);
        return -1;
    }
    flags &= ~O_NONBLOCK;
    if (nb) flags |= O_NONBLOCK;
    int status = fcntl(sk, F_SETFL, flags);
    if (status < 0) {
        ALOGE("Can't set socket to nonblocking mode with fcntl(): %s (%d)", strerror(errno), errno);
        close(sk);
        return -1;
    }
    return 0;
}

static int do_accept(int ag_fd, int out_fd, int out_address, int out_channel) { 
    if (set_nb(ag_fd, true) < 0)
        return -1;
    struct sockaddr_rc raddr;
    int alen = sizeof(raddr);
    int nsk = TEMP_FAILURE_RETRY(accept(ag_fd, (struct sockaddr *) &raddr, &alen));
    if (nsk < 0) {
        ALOGE("Error on accept from socket fd %d: %s (%d).", ag_fd, strerror(errno), errno);
        set_nb(ag_fd, false);
        return -1;
    }

    //env->SetIntField(object, out_fd, nsk);
    //env->SetIntField(object, out_channel, raddr.rc_channel);

    char addr[BTADDR_SIZE];
    get_bdaddr_as_string(&raddr.rc_bdaddr, addr);
    //env->SetObjectField(object, out_address, addr.string());

    ALOGI("Successful accept() on AG socket %d: new socket %d, address %s, RFCOMM channel %d", ag_fd, nsk, addr, raddr.rc_channel);
    set_nb(ag_fd, false);
    return 0;
}

static bool waitForHandsfreeConnectNative(int timeout_ms) {
    //env->SetIntField(object, field_mTimeoutRemainingMs, timeout_ms); 
    int n = 0;
    native_data_t *nat = NULL;
    struct pollfd fds[2];
    int cnt = 0;
    if (nat->hf_ag_rfcomm_channel > 0) {
//        ALOGI("Setting HF AG server socket %d to RFCOMM port %d!", nat->hf_ag_rfcomm_sock, nat->hf_ag_rfcomm_channel);
        fds[cnt].fd = nat->hf_ag_rfcomm_sock;
        fds[cnt].events = POLLIN | POLLPRI | POLLOUT | POLLERR;
        cnt++;
    }
    if (nat->hs_ag_rfcomm_channel > 0) {
//        ALOGI("Setting HS AG server socket %d to RFCOMM port %d!", nat->hs_ag_rfcomm_sock, nat->hs_ag_rfcomm_channel);
        fds[cnt].fd = nat->hs_ag_rfcomm_sock;
        fds[cnt].events = POLLIN | POLLPRI | POLLOUT | POLLERR;
        cnt++;
    }
    if (cnt == 0) {
        ALOGE("Neither HF nor HS listening sockets are open!");
        return FALSE;
    }
    n = TEMP_FAILURE_RETRY(poll(fds, cnt, timeout_ms));
    if (n <= 0) {
        if (n < 0)  {
            ALOGE("listening poll() on RFCOMM sockets: %s (%d)", strerror(errno), errno);
        }
        else {
            //env->SetIntField(object, field_mTimeoutRemainingMs, 0);
//            ALOGI("listening poll() on RFCOMM socket timed out");
        }
        return FALSE;
    }

    //ALOGI("listening poll() on RFCOMM socket returned %d", n);
    int err = 0;
    for (cnt = 0; cnt < (int)(sizeof(fds)/sizeof(fds[0])); cnt++) {
        //ALOGI("Poll on fd %d revent = %d.", fds[cnt].fd, fds[cnt].revents);
        if (fds[cnt].fd == nat->hf_ag_rfcomm_sock) {
            if (fds[cnt].revents & (POLLIN | POLLPRI | POLLOUT)) {
                ALOGI("Accepting HF connection.\n");
                err += do_accept(fds[cnt].fd, 0, 0, 0); //field_mConnectingHandsfreeSocketFd, field_mConnectingHandsfreeAddress, field_mConnectingHandsfreeRfcommChannel);
                n--;
            }
        }
        else if (fds[cnt].fd == nat->hs_ag_rfcomm_sock) {
            if (fds[cnt].revents & (POLLIN | POLLPRI | POLLOUT)) {
                ALOGI("Accepting HS connection.\n");
                err += do_accept(fds[cnt].fd, 0, 0, 0); //field_mConnectingHeadsetSocketFd, field_mConnectingHeadsetAddress, field_mConnectingHeadsetRfcommChannel);
                n--;
            }
        }
    } /* for */

    if (n != 0) {
        ALOGI("Bogus poll(): %d fake pollfd entrie(s)!", n);
        return FALSE;
    } 
    return !err ? TRUE : FALSE;
}

static int setup_listening_socket(int dev, int channel) {
    struct sockaddr_rc laddr;
    int sk, lm; 
    sk = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (sk < 0) {
        ALOGE("Can't create RFCOMM socket");
        return -1;
    } 
    if (debug_no_encrypt()) {
        lm = RFCOMM_LM_AUTH;
    } else {
        lm = RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT;
    } 
    if (lm && setsockopt(sk, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm)) < 0) {
        ALOGE("Can't set RFCOMM link mode");
        close(sk);
        return -1;
    } 
    laddr.rc_family = AF_BLUETOOTH;
    bdaddr_t any = android_bluetooth_bdaddr_any();
    memcpy(&laddr.rc_bdaddr, &any, sizeof(bdaddr_t));
    laddr.rc_channel = channel; 
    if (bind(sk, (struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
        ALOGE("Can't bind RFCOMM socket");
        close(sk);
        return -1;
    } 
    listen(sk, 10);
    return sk;
}

static bool setUpListeningSocketsNative() {
    native_data_t *nat = NULL;
    nat->hf_ag_rfcomm_sock = setup_listening_socket(nat->hcidev, nat->hf_ag_rfcomm_channel);
    if (nat->hf_ag_rfcomm_sock < 0)
        return FALSE; 
    nat->hs_ag_rfcomm_sock = setup_listening_socket(nat->hcidev, nat->hs_ag_rfcomm_channel);
    if (nat->hs_ag_rfcomm_sock < 0) {
        close(nat->hf_ag_rfcomm_sock);
        nat->hf_ag_rfcomm_sock = -1;
        return FALSE;
    }
    return TRUE;
}

/*
    private native void tearDownListeningSocketsNative();
*/
static void tearDownListeningSocketsNative() {
    native_data_t *nat = NULL;

    if (nat->hf_ag_rfcomm_sock > 0) {
        if (close(nat->hf_ag_rfcomm_sock) < 0) {
            ALOGE("Could not close HF server socket: %s (%d)\n", strerror(errno), errno);
        }
        nat->hf_ag_rfcomm_sock = -1;
    }
    if (nat->hs_ag_rfcomm_sock > 0) {
        if (close(nat->hs_ag_rfcomm_sock) < 0) {
            ALOGE("Could not close HS server socket: %s (%d)\n", strerror(errno), errno);
        }
        nat->hs_ag_rfcomm_sock = -1;
    }
}

/* Keep TYPE_RFCOMM etc in sync with BluetoothSocket.java */
static const int TYPE_RFCOMM = 1;
static const int TYPE_SCO = 2;
static const int TYPE_L2CAP = 3;  // TODO: Test l2cap code paths

static const int RFCOMM_SO_SNDBUF = 70 * 1024;  // 70 KB send buffer

static void abortNative();
static void destroyNative();

static struct asocket *get_socketData() {
    struct asocket *s = (struct asocket *) NULL;//env->GetIntField(obj, field_mSocketData);
    return s;
}

static void initSocketFromFdNative(int fd) {
    struct asocket *s = asocket_init(fd);
    if (!s) {
        ALOGV("asocket_init() failed, throwing");
        return;
    }
    //env->SetIntField(obj, field_mSocketData, (int)s);
}

static void initSocketNative() {
    int fd;
    int lm = 0;
    int sndbuf;
    bool auth;
    bool encrypt;
    int type; 
    //type = //env->GetIntField(obj, field_mType); 
    switch (type) {
    case TYPE_RFCOMM:
        fd = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        break;
    case TYPE_SCO:
        fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
        break;
    case TYPE_L2CAP:
        fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
        break;
    default:
        return;
    } 
    if (fd < 0) {
        ALOGV("socket() failed, throwing");
        return;
    } 
    //auth = env->GetBooleanField(obj, field_mAuth);
    //encrypt = env->GetBooleanField(obj, field_mEncrypt); 
    /* kernel does not yet support LM for SCO */
    switch (type) {
    case TYPE_RFCOMM:
        lm |= auth ? RFCOMM_LM_AUTH : 0;
        lm |= encrypt ? RFCOMM_LM_ENCRYPT : 0;
        lm |= (auth && encrypt) ? RFCOMM_LM_SECURE : 0;
        break;
    case TYPE_L2CAP:
        lm |= auth ? L2CAP_LM_AUTH : 0;
        lm |= encrypt ? L2CAP_LM_ENCRYPT : 0;
        lm |= (auth && encrypt) ? L2CAP_LM_SECURE : 0;
        break;
    } 
    if (lm) {
        if (setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
            ALOGV("setsockopt(RFCOMM_LM) failed, throwing");
            return;
        }
    } 
    if (type == TYPE_RFCOMM) {
        sndbuf = RFCOMM_SO_SNDBUF;
        if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
            ALOGV("setsockopt(SO_SNDBUF) failed, throwing");
            return;
        }
    } 
    ALOGV("...fd %d created (%s, lm = %x)", fd, TYPE_AS_STR(type), lm); 
    initSocketFromFdNative(fd);
}

static void connectNative() {
    int ret;
    int type;
    const char *c_address;
    String8 address;
    bdaddr_t bdaddress;
    socklen_t addr_sz;
    struct sockaddr *addr;
    struct asocket *s = get_socketData();
    int retry = 0;

    if (!s)
        return; 
    //type = env->GetIntField(obj, field_mType); 
    /* parse address into bdaddress */
    //address = (String8) env->GetObjectField(obj, field_mAddress);
    c_address = address.string();
    if (get_bdaddr(c_address, &bdaddress)) {
        //env->ReleaseStringUTFChars(address, c_address);
        return;
    }
    //env->ReleaseStringUTFChars(address, c_address);

    switch (type) {
    case TYPE_RFCOMM:
        struct sockaddr_rc addr_rc;
        addr = (struct sockaddr *)&addr_rc;
        addr_sz = sizeof(addr_rc); 
        memset(addr, 0, addr_sz);
        addr_rc.rc_family = AF_BLUETOOTH;
        //addr_rc.rc_channel = env->GetIntField(obj, field_mPort);
        memcpy(&addr_rc.rc_bdaddr, &bdaddress, sizeof(bdaddr_t)); 
        break;
    case TYPE_SCO:
        struct sockaddr_sco addr_sco;
        addr = (struct sockaddr *)&addr_sco;
        addr_sz = sizeof(addr_sco); 
        memset(addr, 0, addr_sz);
        addr_sco.sco_family = AF_BLUETOOTH;
        memcpy(&addr_sco.sco_bdaddr, &bdaddress, sizeof(bdaddr_t)); 
        break;
    case TYPE_L2CAP:
        struct sockaddr_l2 addr_l2;
        addr = (struct sockaddr *)&addr_l2;
        addr_sz = sizeof(addr_l2); 
        memset(addr, 0, addr_sz);
        addr_l2.l2_family = AF_BLUETOOTH;
        //addr_l2.l2_psm = env->GetIntField(obj, field_mPort);
        memcpy(&addr_l2.l2_bdaddr, &bdaddress, sizeof(bdaddr_t)); 
        break;
    default:
        return;
    } 
connect:
    ret = asocket_connect(s, addr, addr_sz, -1);
    ALOGV("...connect(%d, %s) = %d (errno %d)", s->fd, TYPE_AS_STR(type), ret, errno); 
    if (ret && errno == EALREADY && retry < 2) {
        /* workaround for bug 5082381 (EALREADY on ACL collision):
         * retry the connect. Unfortunately we have to create a new fd.
         * It's not ideal to switch the fd underneath the object, but
         * is currently safe */
        ALOGD("Hit bug 5082381 (EALREADY on ACL collision), trying workaround");
        usleep(100000);
        retry++;
        initSocketNative();
        goto connect;
    }
    if (!ret && retry > 0)
        ALOGD("...workaround ok"); 
}

/* Returns errno instead of throwing, so java can check errno */
static int bindListenNative() {
    int type;
    socklen_t addr_sz;
    struct sockaddr *addr;
    bdaddr_t bdaddr = android_bluetooth_bdaddr_any();
    struct asocket *s = get_socketData();
    if (!s)
        return EINVAL; 
    //type = env->GetIntField(obj, field_mType); 
    switch (type) {
    case TYPE_RFCOMM:
        struct sockaddr_rc addr_rc;
        addr = (struct sockaddr *)&addr_rc;
        addr_sz = sizeof(addr_rc); 
        memset(addr, 0, addr_sz);
        addr_rc.rc_family = AF_BLUETOOTH;
        //addr_rc.rc_channel = env->GetIntField(obj, field_mPort);
        memcpy(&addr_rc.rc_bdaddr, &bdaddr, sizeof(bdaddr_t));
        break;
    case TYPE_SCO:
        struct sockaddr_sco addr_sco;
        addr = (struct sockaddr *)&addr_sco;
        addr_sz = sizeof(addr_sco); 
        memset(addr, 0, addr_sz);
        addr_sco.sco_family = AF_BLUETOOTH;
        memcpy(&addr_sco.sco_bdaddr, &bdaddr, sizeof(bdaddr_t));
        break;
    case TYPE_L2CAP:
        struct sockaddr_l2 addr_l2;
        addr = (struct sockaddr *)&addr_l2;
        addr_sz = sizeof(addr_l2); 
        memset(addr, 0, addr_sz);
        addr_l2.l2_family = AF_BLUETOOTH;
        //addr_l2.l2_psm = env->GetIntField(obj, field_mPort);
        memcpy(&addr_l2.l2_bdaddr, &bdaddr, sizeof(bdaddr_t));
        break;
    default:
        return ENOSYS;
    } 
    if (bind(s->fd, addr, addr_sz)) {
        ALOGV("...bind(%d) gave errno %d", s->fd, errno);
        return errno;
    } 
    if (listen(s->fd, 1)) {
        ALOGV("...listen(%d) gave errno %d", s->fd, errno);
        return errno;
    }
    ALOGV("...bindListenNative(%d) success", s->fd);
    return 0;
}

static int *acceptNative(int timeout) {
    int fd;
    int type;
    struct sockaddr *addr;
    socklen_t addr_sz;
    String8 addr_jstr;
    char addr_cstr[BTADDR_SIZE];
    bdaddr_t *bdaddr;
    bool auth;
    bool encrypt; 
    struct asocket *s = get_socketData();
    if (!s)
        return NULL; 
    //type = env->GetIntField(obj, field_mType); 
    switch (type) {
    case TYPE_RFCOMM:
        struct sockaddr_rc addr_rc;
        addr = (struct sockaddr *)&addr_rc;
        addr_sz = sizeof(addr_rc);
        bdaddr = &addr_rc.rc_bdaddr;
        memset(addr, 0, addr_sz);
        break;
    case TYPE_SCO:
        struct sockaddr_sco addr_sco;
        addr = (struct sockaddr *)&addr_sco;
        addr_sz = sizeof(addr_sco);
        bdaddr = &addr_sco.sco_bdaddr;
        memset(addr, 0, addr_sz);
        break;
    case TYPE_L2CAP:
        struct sockaddr_l2 addr_l2;
        addr = (struct sockaddr *)&addr_l2;
        addr_sz = sizeof(addr_l2);
        bdaddr = &addr_l2.l2_bdaddr;
        memset(addr, 0, addr_sz);
        break;
    default:
        return NULL;
    } 
    fd = asocket_accept(s, addr, &addr_sz, timeout); 
    ALOGV("...accept(%d, %s) = %d (errno %d)", s->fd, TYPE_AS_STR(type), fd, errno); 
    if (fd < 0) {
        return NULL;
    } 
    /* Connected - return new BluetoothSocket */
    //auth = env->GetBooleanField(obj, field_mAuth);
    //encrypt = env->GetBooleanField(obj, field_mEncrypt); 
    get_bdaddr_as_string(bdaddr, addr_cstr); 
    addr_jstr = String8(addr_cstr);
    //return env->NewObject(class_BluetoothSocket, method_BluetoothSocket_ctor, type, fd, auth, encrypt, addr_jstr, -1);
    return NULL;
}

static int availableNative() {
    int available;
    struct asocket *s = get_socketData();
    if (!s)
        return -1; 
    if (ioctl(s->fd, FIONREAD, &available) < 0) {
        return -1;
    } 
    return available;
}

static int readNative(char * jb, int offset, int length) {
    int ret;
    char *b;
    int sz;
    struct asocket *s = get_socketData();
    if (!s)
        return -1;
    if (jb == NULL) {
        return -1;
    }
    //sz = env->GetArrayLength(jb);
    if (offset < 0 || length < 0 || offset + length > sz) {
        return -1;
    } 
    //b = env->GetByteArrayElements(jb, NULL);
    if (b == NULL) {
        return -1;
    } 
    ret = asocket_read(s, &b[offset], length, -1);
    if (ret < 0) {
        return -1;
    } 
    return (int)ret;
}

static int writeNative(char * jb, int offset, int length) {
    int ret, total;
    char *b;
    int sz;
    struct asocket *s = get_socketData();
    if (!s)
        return -1;
    if (jb == NULL) {
        return -1;
    }
    //sz = env->GetArrayLength(jb);
    if (offset < 0 || length < 0 || offset + length > sz) {
        return -1;
    } 
    //b = env->GetByteArrayElements(jb, NULL);
    if (b == NULL) {
        return -1;
    } 
    total = 0;
    while (length > 0) {
        ret = asocket_write(s, &b[offset], length, -1);
        if (ret < 0) {
            return -1;
        }
        offset += ret;
        total += ret;
        length -= ret;
    } 
    return (int)total;
}

static void abortNative() {
    struct asocket *s = get_socketData();
    if (!s)
        return;
    asocket_abort(s);
    ALOGV("...asocket_abort(%d) complete", s->fd);
}

static void destroyNative() {
    struct asocket *s = get_socketData();
    int fd = s->fd;
    if (!s)
        return;
    asocket_destroy(s);
    ALOGV("...asocket_destroy(%d) complete", fd);
}
} /* namespace android */

