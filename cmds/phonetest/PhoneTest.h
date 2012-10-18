#ifndef _SIGYN_TEST_H
#define _SIGYN_TEST_H

#include <sigyn/ISigynClient.h>
#include <sigyn/ISigynService.h>

class SigynClient : public android::BnSigynClient
{
public:
  virtual void stateChanged(int32_t state,int32_t token,int32_t reason, const android::String16&  number);
  virtual void rssiChanged(int32_t rssi) ;
  virtual void operatorChanged(const android::String16& name) ;
};

// -------------------------------------------------------------------

class SigynMaster : public android::IBinder::DeathRecipient
{
public:
    static android::sp<SigynMaster>& instance();
    virtual ~SigynMaster();

    void connect(android::ConnectFlags flag);
    void call(const android::String16& number);

private:
    SigynMaster();
    virtual void onFirstRef();
    virtual void binderDied(const android::wp<android::IBinder>& who);

private:
    android::sp<SigynClient>            mSigynClient;
    android::sp<android::ISigynService> mSigynService;
};

#endif
