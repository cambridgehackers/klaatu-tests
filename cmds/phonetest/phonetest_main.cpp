#include <stdio.h>
#include <getopt.h>
#include <phone/PhoneClient.h>
#include <binder/ProcessState.h>

using namespace android;

// -------------------------------------------------------------------

static const char * ril_error_to_string(int err)
{
    switch (err) {
    case RIL_E_SUCCESS: return "Okay";
    case RIL_E_RADIO_NOT_AVAILABLE: return "Radio not available";
    case RIL_E_GENERIC_FAILURE: return "Generic failure";
    case RIL_E_PASSWORD_INCORRECT: return "Password incorrect";
    case RIL_E_SIM_PIN2: return "Operation requires SIM PIN2 to be entered [4]";
    case RIL_E_SIM_PUK2: return "Operation requires SIM PIN2 to be entered [5]";
    case RIL_E_REQUEST_NOT_SUPPORTED: return "Request not supported";
    case RIL_E_CANCELLED: return "Cancelled";
    case RIL_E_OP_NOT_ALLOWED_DURING_VOICE_CALL: return "Operation not allowed during voice call";
    case RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW: return "Device needs to register in network";
    case RIL_E_SMS_SEND_FAIL_RETRY: return "Failed to send SMS; need retry";
    case RIL_E_SIM_ABSENT: return "SIM or RUIM card absent";
    case RIL_E_SUBSCRIPTION_NOT_AVAILABLE: return "Fail to find CDMA subscription";
    case RIL_E_MODE_NOT_SUPPORTED: return "HW does not support preferred network type";
    case RIL_E_FDN_CHECK_FAILURE: return "Command failed because recipient not on FDN list";
    case RIL_E_ILLEGAL_SIM_OR_ME: return "Network selection failed to due illeal SIM or ME";
    default:
	return "Unknown";
    }
}

static bool checkresult(const char *str, int result)
{
    if (result) {
	printf("%s failed: '%s' (%d)\n", str, ril_error_to_string(result), result);
	return false;
    }
    return true;
}

// -------------------------------------------------------------------

class MyClient : public PhoneClient
{
protected:
    virtual void ResponseGetSIMStatus(int token, int result, bool simCardPresent) {
	if (checkresult("SIM status", result))
	    printf("SIM Status simCardPresent=%d\n", simCardPresent);
    }

    virtual void ResponseGetCurrentCalls(int token, int result, const List<CallState>& calls) {
	if (checkresult("GetCurrentCalls", result)) {
	    printf("%d current call(s)\n", calls.size());
	    for (List<CallState>::const_iterator it = calls.begin() ; it != calls.end() ; ++it) {
		printf(" state=%d index=%d number=%s name=%s\n", it->state, it->index, 
		       String8(it->number).string(), String8(it->name).string());
	    }
	}
    }

    virtual void ResponseDial(int token, int result) {
	if (checkresult("Dial", result))
	    printf("Dial succeeded\n");
    }

    virtual void ResponseHangup(int token, int result) {
	if (checkresult("Hangup", result))
	    printf("Hangup succeeded\n");
    }

    virtual void ResponseReject(int token, int result) {
	if (checkresult("Reject", result))
	    printf("Reject succeeded\n");
    }

    virtual void ResponseOperator(int token, int result, const String16& longONS,
				  const String16& shortONS, const String16& code) {
	if (checkresult("Operator", result))
	    printf("Operator longONS='%s' shortONS='%s' code='%s'\n",
		   String8(longONS).string(), String8(shortONS).string(), String8(code).string());
    }

    virtual void ResponseAnswer(int token, int result) {
	if (checkresult("Answer", result))
	    printf("Answer succeeded\n");
    }

    virtual void ResponseVoiceRegistrationState(int token, int result, const List<String16>& strlist) {
	if (checkresult("VoiceRegistrationState", result)) {
	    printf("VoiceRegistrationState size=%d\n", strlist.size());
	    for (List<String16>::const_iterator it = strlist.begin() ; it != strlist.end() ; ++it)
		printf(" '%s'\n", String8(*it).string());
	}
    }

    virtual void UnsolicitedRadioStateChanged(int state) {
	printf("Unsolicited radio state changed to %d\n", state);
    }

    virtual void UnsolicitedCallStateChanged() {
	printf("Unsolicited call state changed\n");
	GetCurrentCalls(1000);
    }

    virtual void UnsolicitedVoiceNetworkStateChanged() {
	printf("Unsolicited voice network state changed\n");
	printf("...checking voice registration state and operator\n");
	Operator(1001);
	VoiceRegistrationState(1001);
    }

    virtual void UnsolicitedNITZTimeReceived(const String16& time) {
	printf("Unsolicited time received: %s\n", String8(time).string());
    }

    virtual void UnsolicitedSignalStrength(int rssi) {
	printf("Unsolicited signal strength = %d\n", rssi);
    }
};


// -------------------------------------------------------------------

static void usage(const char *progname)
{
    printf("Usage:  %s [OPT+] CMD [ARGS+]\n"
	   "\n"
	   "   -f               Display unsolicited commands.\n"
	   "                    The phonetest program will not terminate\n"
	   "\n"
	   "   status           Display the current call status\n"
	   "   current          Display current calls\n"
	   "   operator         Retrieve operator information\n"
	   "   dial NUMBER      Make a phone call.\n"
	   "   hangup NUMBER    Hangup an active call\n"
	   "   answer           Answer an incoming call\n"
	   "   reject           Reject an incoming call\n"
	   , progname);
    exit(0);
}

int main(int argc, char **argv)
{
    bool follow = false;

    int res;
    while ((res = getopt(argc, argv, "fh")) >= 0) {
	switch (res) {
	case 'f':
	    follow = true;
	    break;
	case 'h':
	default:
	    usage(argv[0]);
	    break;
	}
    }

    if (optind >= argc && !follow) {
	printf("No command specified\n");
	usage(argv[0]);
    }

    ProcessState::self()->startThreadPool();
    android::sp<MyClient> s = new MyClient();
    int token = 0;

    if (follow)
	s->Register(UM_ALL);

    if (optind < argc) {
	if (strcmp(argv[optind], "status") == 0) {
	    s->GetSIMStatus(token++);
	}
	else if (strcmp(argv[optind], "current") == 0) {
	    s->GetCurrentCalls(token++);
	}
	else if (strcmp(argv[optind], "operator") == 0) {
	    s->Operator(token++);
	}
	else if (strcmp(argv[optind], "dial") == 0) {
	    optind++;
	    if (optind >= argc) {
		printf("Need to specifiy a number to dial\n");
		usage(argv[0]);
	    }
	    s->Dial(token++, String16(argv[optind]));
	} 
	else if (strcmp(argv[optind], "hangup") == 0) {
	    optind++;
	    if (optind >= argc) {
		printf("Need to specifiy a call index to hangup\n");
		usage(argv[0]);
	    }
	    s->Hangup(token++, atoi(argv[optind]));
	} 
	else if (strcmp(argv[optind], "answer") == 0) {
	    s->Answer(token++);
	} 
	else if (strcmp(argv[optind], "reject") == 0) {
	    s->Reject(token++);
	} 
	else {
	    printf("Unrecognized command '%s'\n", argv[optind]);
	    usage(argv[0]);
	}
    }

    while (follow || s->pending()) {
	sleep(1);
    }
}
