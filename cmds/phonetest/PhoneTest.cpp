/*
  Very simple binder client program to see of the bidirctional binder stuff is working.
  Hopefully this will get fleshed out enought to make a call.
 */

#include "PhoneTest.h"

#include <binder/IServiceManager.h>
#include <loki/ILokiClient.h>
#include <loki/ILokiService.h>
#include <binder/ProcessState.h>

using namespace android;

void PhoneClient::stateChanged(int32_t state,int32_t token,int32_t reason, const String16& number)
{
  printf("%s:%d stateChanged \n",__FILE__,__LINE__);
}

void PhoneClient::rssiChanged(int32_t rssi)
{
  printf("%s:%d rssiChanged \n",__FILE__,__LINE__);
}

void PhoneClient::operatorChanged(const String16& name)
{
  printf("%s:%d operatorChanged \n",__FILE__,__LINE__);
}

// -------------------------------------------------------------------

sp<PhoneMaster> s_phonemaster;

sp<PhoneMaster>& PhoneMaster::instance()
{
    if (!s_phonemaster.get())
	s_phonemaster = new PhoneMaster;
    return s_phonemaster;
}

PhoneMaster::PhoneMaster()
{
}

PhoneMaster::~PhoneMaster()
{
}

/*
  This initialization stuff really has to be done in onFirstRef.
  If you try to do it inside of the constructor, bad things
  happen.
 */

void PhoneMaster::onFirstRef()
{
    ProcessState::self()->startThreadPool();

    sp<IServiceManager> sm     = defaultServiceManager();
    sp<IBinder>         binder = sm->getService(String16("phone"));

    status_t err = binder->linkToDeath(this, 0);
    if (err) {
	printf("Unable to link to phone death: %s", strerror(-err));
	exit(-1);
    }
  
    mPhoneService = interface_cast<IPhoneService>(binder);
    fprintf(stderr,"%s:%d onFirstRef done\n",__FILE__,__LINE__);
}

void PhoneMaster::binderDied(const wp<IBinder>& who)
{
    printf("%s:%d Phone died \n",__FILE__,__LINE__);
    exit(-1);
}

void PhoneMaster::connect(ConnectFlags flag)
{
    printf("%s:%d connecting to phone service  \n",__FILE__,__LINE__);
    mPhoneClient = new PhoneClient();
    mPhoneService->connect(mPhoneClient, flag);
}

void PhoneMaster::call(const String16& number)
{
    printf("%s:%d one day i will call \n",__FILE__,__LINE__);
    mPhoneService->call(mPhoneClient, number);
}

