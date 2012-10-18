#ifndef _PHONE_TEST_H
#define _PHONE_TEST_H

#include <phone/IPhoneClient.h>
#include <phone/IPhoneService.h>

class PhoneClient : public android::BnPhoneClient
{
public:
  virtual void stateChanged(int32_t state,int32_t token,int32_t reason, const android::String16&  number);
  virtual void rssiChanged(int32_t rssi) ;
  virtual void operatorChanged(const android::String16& name) ;
};

// -------------------------------------------------------------------

class PhoneMaster : public android::IBinder::DeathRecipient
{
public:
    static android::sp<PhoneMaster>& instance();
    virtual ~PhoneMaster();

    void connect(android::ConnectFlags flag);
    void call(const android::String16& number);

private:
    PhoneMaster();
    virtual void onFirstRef();
    virtual void binderDied(const android::wp<android::IBinder>& who);

private:
    android::sp<PhoneClient>            mPhoneClient;
    android::sp<android::IPhoneService> mPhoneService;
};

#endif
