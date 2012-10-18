/*
  Very simple binder client program to see of the bidirctional binder stuff is working.
  Hopefully this will get fleshed out enought to make a call.
 */

#include "SigynTest.h"

#include <binder/IServiceManager.h>
#include <loki/ILokiClient.h>
#include <loki/ILokiService.h>
#include <binder/ProcessState.h>

using namespace android;

void SigynClient::stateChanged(int32_t state,int32_t token,int32_t reason, const String16& number)
{
  printf("%s:%d stateChanged \n",__FILE__,__LINE__);
}

void SigynClient::rssiChanged(int32_t rssi)
{
  printf("%s:%d rssiChanged \n",__FILE__,__LINE__);
}

void SigynClient::operatorChanged(const String16& name)
{
  printf("%s:%d operatorChanged \n",__FILE__,__LINE__);
}

// -------------------------------------------------------------------

sp<SigynMaster> s_sigynmaster;

sp<SigynMaster>& SigynMaster::instance()
{
    if (!s_sigynmaster.get())
	s_sigynmaster = new SigynMaster;
    return s_sigynmaster;
}

SigynMaster::SigynMaster()
{
}

SigynMaster::~SigynMaster()
{
}

/*
  This initialization stuff really has to be done in onFirstRef.
  If you try to do it inside of the constructor, bad things
  happen.
 */

void SigynMaster::onFirstRef()
{
    ProcessState::self()->startThreadPool();

    sp<IServiceManager> sm     = defaultServiceManager();
    sp<IBinder>         binder = sm->getService(String16("sigyn"));

    status_t err = binder->linkToDeath(this, 0);
    if (err) {
	printf("Unable to link to sigyn death: %s", strerror(-err));
	exit(-1);
    }
  
    mSigynService = interface_cast<ISigynService>(binder);
    fprintf(stderr,"%s:%d onFirstRef done\n",__FILE__,__LINE__);
}

void SigynMaster::binderDied(const wp<IBinder>& who)
{
    printf("%s:%d Sigyn died \n",__FILE__,__LINE__);
    exit(-1);
}

void SigynMaster::connect(ConnectFlags flag)
{
    printf("%s:%d connecting to sigyn service  \n",__FILE__,__LINE__);
    mSigynClient = new SigynClient();
    mSigynService->connect(mSigynClient, flag);
}

void SigynMaster::call(const String16& number)
{
    printf("%s:%d one day i will call \n",__FILE__,__LINE__);
    mSigynService->call(mSigynClient, number);
}

