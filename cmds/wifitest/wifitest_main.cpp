#include <stdio.h>
#include <wifi/WifiClient.h>
#include <binder/ProcessState.h>

using namespace android;

// -------------------------------------------------------------------

class MyClient : public WifiClient
{
    void State(WifiState state) {
	printf("State: %d\n", state);
    }

    void ScanResults(const Vector<ScannedStation>& scandata) {
	printf("ScanResults\n");
	for (size_t i = 0 ; i < scandata.size() ; i++) {
	    const ScannedStation& s(scandata[i]);
	    printf("  %s %s %s %d %d\n", 
		   s.bssid.string(), 
		   s.ssid.string(),
		   s.flags.string(),
		   s.frequency,
		   s.rssi);
	}
    }

    void ConfiguredStations(const Vector<ConfiguredStation>& configdata) {
	printf("ConfiguredStations\n");
	printf("  ID SSID         Pri KeyMgmt    Key Status\n");
	for (size_t i = 0 ; i < configdata.size() ; i++) {
	    const ConfiguredStation& c(configdata[i]);
	    printf("  %2d %-12s %3d %-12s %1s %s\n", 
		   c.network_id, c.ssid.string(),c.priority,
		   c.key_mgmt.string(), c.pre_shared_key.string(),
		   (c.status == ConfiguredStation::CURRENT ? "Current" : 
		    (c.status == ConfiguredStation::ENABLED ? "Enabled" : "Disabled")));
	}
    }

    void Information(const WifiInformation& info) {
	printf("Information\n"
	       "  macaddr: %s\n"
	       "   ipaddr: %s\n"
	       "    bssid: %s\n"
	       "     ssid: %s\n"
	       "       id: %d\n"
	       "    state: %s (%d)\n"
	       "     rssi: %d\n"
	       "    speed: %d\n",
	       info.macaddr.string(), info.ipaddr.string(), info.bssid.string(),
	       info.ssid.string(), info.network_id, 
	       WifiClient::supStateToString(info.supplicant_state),
	       info.supplicant_state,
	       info.rssi, info.link_speed
	    );
	
    }

    void Rssi(int rssi) {
	printf("rssi: %d\n", rssi);
    }
    void LinkSpeed(int link_speed) {
	printf("link speed: %d\n", link_speed);
    }
};


// -------------------------------------------------------------------

static void usage(const char *progname)
{
    printf("Usage:  %s [CMD]*\n"
	   "\n"
	   "   monitor             Do not terminate; continue displaying data\n"
	   "                       If no command is specified, this is the default\n"
	   "   driver on|off       Driver on or off\n"
	   "   scan                Run an RSSI scan (passive)\n"
	   "   activescan          Run an RSSI scan, active\n"
	   "   polling on|off      RSSI polling while connected\n"
	   "   background on|off   Background scanning while disconnected\n"
	   "   add SSID[,PASSWD]   Add network with SSID and optional password\n"
	   "   remove ID           Remove network ID\n"
	   "   enable ID           Enable network ID\n"
	   "   select ID           Select network ID (disable all others)\n"
	   "   disable ID          Disable network ID\n"
	   "   reconnect           Reconnect to the current network\n"
	   "   disconnect          Disconnect from the current network\n"
	   "   reassociate         Reassociate with the current network\n"
	   "\n"
	   "On|Off values can be specified as true/false, on/off, or 1/0\n"
	   , progname);
    exit(0);
}

bool getBoolean(int index, int argc, char **argv) 
{
    if (index >= argc) {
	printf("Command %s takes an argument\n", argv[index-1]);
	usage(argv[0]);
    }
    return (!strcasecmp(argv[index], "on") || 
	    !strcasecmp(argv[index], "true") ||
	    *argv[index] == '1');
}

int getInt(int index, int argc, char **argv) 
{
    if (index >= argc) {
	printf("Command %s takes an argument\n", argv[index-1]);
	usage(argv[0]);
    }
    return atoi(argv[index]);
}

static Vector<String8> _splitString(const char *start, char token, unsigned int max_values)
{
    Vector<String8> result;
    if (*start) {
	const char *ptr = start;
	while (result.size() < max_values - 1) {
	    while (*ptr != token && *ptr != '\0')
		ptr++;
	    result.push(String8(start, ptr-start));
	    if (*ptr == '\0')
		break;
	    ptr++;
	    start = ptr;
	}
	if (*ptr != '\0')
	    result.push(String8(ptr));
    }
    return result;
}

/* Return a list of comma separated strings */
Vector<String8> getStrings(int index, int argc, char **argv) 
{
    if (index >= argc) {
	printf("Command %s takes an argument\n", argv[index-1]);
	usage(argv[0]);
    }
    return _splitString(argv[index], ',', 2);
}


int main(int argc, char **argv)
{
    bool follow = false;
    bool wait_for_config = false;

    ProcessState::self()->startThreadPool();
    android::sp<MyClient> s = new MyClient();

    if (argc == 1) {
	s->Register(WIFI_CLIENT_FLAG_ALL);
	follow = true;
    }
    else {
	int i = 1;
	while (i < argc) {
	    if (!strcmp(argv[i], "monitor")) {
		s->Register(WIFI_CLIENT_FLAG_ALL);
		follow = true;
	    }
	    else if (!strcmp(argv[i], "driver"))
		s->SetEnabled(getBoolean(++i, argc, argv));
	    else if (!strcmp(argv[i], "scan")) 
		s->StartScan(false);
	    else if (!strcmp(argv[i], "activescan"))
		s->StartScan(true);
	    else if (!strcmp(argv[i], "polling"))
		s->EnableRssiPolling(getBoolean(++i, argc, argv));
	    else if (!strcmp(argv[i], "background"))
		s->EnableBackgroundScan(getBoolean(++i, argc, argv));
	    else if (!strcmp(argv[i], "add")) {
		Vector<String8> slist = getStrings(++i, argc, argv);
		ConfiguredStation cs;
		cs.ssid = slist[0];
		if (slist.size() >= 2) {
		    cs.key_mgmt = "WPA-PSK";
		    cs.pre_shared_key = slist[1];
		}
		else {
		    cs.key_mgmt = "NONE";
		}
		s->AddOrUpdateNetwork(cs);
	    }
	    else if (!strcmp(argv[i], "remove"))
		s->RemoveNetwork(getInt(++i, argc, argv));
	    else if (!strcmp(argv[i], "enable"))
		s->EnableNetwork(getInt(++i, argc, argv), false);
	    else if (!strcmp(argv[i], "select"))
		s->SelectNetwork(getInt(++i, argc, argv));
	    else if (!strcmp(argv[i], "disable"))
		s->DisableNetwork(getInt(++i, argc, argv));
	    else if (!strcmp(argv[i], "reconnect"))
		s->Reconnect();
	    else if (!strcmp(argv[i], "disconnect"))
		s->Disconnect();
	    else if (!strcmp(argv[i], "reassociate"))
		s->Reassociate();
	    else {
		printf("Unrecognized command '%s'\n", argv[i]);
		usage(argv[0]);
	    }
	    i++;
	}
    }

    while (follow)
	sleep(1);
}
