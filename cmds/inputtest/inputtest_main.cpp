#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <binder/ProcessState.h>
#include <input/EventHub.h>

using namespace android;


// -------------------------------------------------------------------

static struct { 
    const char *name;
    int axis;
} s_abs_axis[] = { 
    {"ABS_MT_POSITION_X", ABS_MT_POSITION_X},
    {"ABS_MT_POSITION_Y", ABS_MT_POSITION_Y},
    {"ABS_MT_TOUCH_MAJOR", ABS_MT_TOUCH_MAJOR},
    {"ABS_MT_TOUCH_MINOR", ABS_MT_TOUCH_MINOR},
    {"ABS_MT_WIDTH_MAJOR", ABS_MT_WIDTH_MAJOR},
    {"ABS_MT_WIDTH_MINOR", ABS_MT_WIDTH_MINOR},
    {"ABS_MT_ORIENTATION", ABS_MT_ORIENTATION},
    {"ABS_MT_PRESSURE", ABS_MT_PRESSURE},
    {"ABS_MT_DISTANCE", ABS_MT_DISTANCE},
    {"ABS_MT_TRACKING_ID", ABS_MT_TRACKING_ID},
    {"ABS_MT_SLOT", ABS_MT_SLOT},
    {0, 0}
};


static void usage()
{
    printf("Usage:  inputtest [CMD...]\n"
	   "\n"
	   "    devices         Show a list of attached devices\n"
	   "\n"
	   "With no commands, inputtest will show raw input events\n"
	   "\n"
	);
    exit(0);
}

const int kRawBufferSize = 100;

static const char *s_abs_name[] = {
    "ABS_X",           // 0
    "ABS_Y",           // 1
    "ABS_Z",           // 2
    "ABS_RX",          // 3
    "ABS_RY",          // 4
    "ABS_RZ",          // 5
    "ABS_THROTTLE",    // 6
    "ABS_RUDDER",      // 7
    "ABS_WHEEL",       // 8
    "ABS_GAS",         // 9
    "ABS_BRAKE",       // 0x0a
    "ABS_0x0b",        // 0x0b
    "ABS_0x0c",        // 0x0c
    "ABS_0x0d",        // 0x0d
    "ABS_0x0e",        // 0x0e
    "ABS_0x0f",        // 0x0f
    "ABS_HAT0X",       // 0x10
    "ABS_HAT0Y",       // 0x11
    "ABS_HAT1X",       // 0x12
    "ABS_HAT1Y",       // 0x13
    "ABS_HAT2X",       // 0x14
    "ABS_HAT2Y",       // 0x15
    "ABS_HAT3X",       // 0x16
    "ABS_HAT3Y",       // 0x17
    "ABS_PRESSURE",    // 0x18
    "ABS_DISTANCE",    // 0x19
    "ABS_TILT_X",      // 0x1a
    "ABS_TILT_Y",      // 0x1b
    "ABS_TOOL_WIDTH",  // 0x1c
    "ABS_0x1d",        // 0x1d
    "ABS_0x1e",        // 0x1e
    "ABS_0x1f",        // 0x1f
    "ABS_VOLUME",      // 0x20
    "ABS_0x21",        
    "ABS_0x22",
    "ABS_0x23",
    "ABS_0x24",
    "ABS_0x25",
    "ABS_0x26",
    "ABS_0x27",
    "ABS_MISC",           // 0x28
    "ABS_0x29",
    "ABS_0x2a",
    "ABS_0x2b",
    "ABS_0x2c",
    "ABS_0x2d",
    "ABS_0x2e",
    "ABS_MT_SLOT",        // 0x2f
    "ABS_MT_TOUCH_MAJOR", // 0x30
    "ABS_MT_TOUCH_MINOR", // 0x31
    "ABS_MT_WIDTH_MAJOR", // 0x32
    "ABS_MT_WIDTH_MINOR", // 0x33
    "ABS_MT_ORIENTATION", // 0x34
    "ABS_MT_POSITION_X",  // 0x35
    "ABS_MT_POSITION_Y",  // 0x36
    "ABS_MT_TOOL_TYPE",   // 0x37
    "ABS_MT_BLOB_ID",     // 0x38
    "ABS_MT_TRACKING_ID", // 0x39
    "ABS_MT_PRESSURE",    // 0x3a
    "ABS_MT_DISTANCE",    // 0x3b
    "ABS_0x3c",
    "ABS_0x3d",
    "ABS_0x3e",
    "ABS_0x3f"
};

void dump_event(const RawEvent& ev)
{
    if (ev.type == EV_ABS && ev.code < ABS_CNT)
	printf("%lld device_id=%d %s 0x%x value=%d\n", 
	       ev.when, ev.deviceId, s_abs_name[ev.code], ev.code, ev.value);
    else 
	printf("%lld device_id=%d type=0x%x code=%d value=%d\n", 
	       ev.when, ev.deviceId, ev.type, ev.code, ev.value);
}

struct ClassToName {
    uint32_t bits;
    const char *name;
} s_class_names[] = { 
    { INPUT_DEVICE_CLASS_KEYBOARD, "keyboard" },
    { INPUT_DEVICE_CLASS_ALPHAKEY, "alphakeyboard" },
    { INPUT_DEVICE_CLASS_TOUCH, "touch" },
    { INPUT_DEVICE_CLASS_CURSOR, "cursor" },
    { INPUT_DEVICE_CLASS_TOUCH_MT, "multitouch" },
    { INPUT_DEVICE_CLASS_DPAD, "directional_pad"},
    { INPUT_DEVICE_CLASS_GAMEPAD, "game_pad"},
    { INPUT_DEVICE_CLASS_SWITCH, "switch" },
    { INPUT_DEVICE_CLASS_JOYSTICK, "joystick" },
    { INPUT_DEVICE_CLASS_EXTERNAL, "external" },
    { 0, "" }
};

void dump_class(uint32_t k) 
{
    int count = 0;
    for (int i = 0 ; s_class_names[i].bits ; i++ )
	if (s_class_names[i].bits & k) 
	    printf((count++ ? " %s" : "%s"), s_class_names[i].name);
}

int main(int argc, char **argv)
{
    bool show_devices  = false;
    bool run_forever   = false;
    bool dump_raw      = false;
    bool finished_scan = false;

    if (argc > 1) {
	if (*(argv[1]) == 'd') {
	    show_devices = true;
	}
	else {
	    usage();
	}
    } else {
	dump_raw = true;
	run_forever = true;
    }

    ProcessState::self()->startThreadPool();
    android::sp<EventHub> hub = new EventHub();

    RawEvent buffer[kRawBufferSize];

    while (!finished_scan || run_forever) {
	size_t n = hub->getEvents(1000, buffer, kRawBufferSize);
	// printf("Got %u events\n", n);
	for (size_t i = 0 ; i < n ; i++) {
	    if (dump_raw)
		dump_event(buffer[i]);
	    if (show_devices) {
		if (buffer[i].type == EventHubInterface::DEVICE_ADDED) {
		    int32_t id = buffer[i].deviceId;
		    InputDeviceIdentifier identifier = hub->getDeviceIdentifier(id);
		    printf("Device id=%d, name='%s' [", id, identifier.name.string());
		    dump_class(hub->getDeviceClasses(id));
		    printf("]\n");

		    PropertyMap config;
		    hub->getConfiguration(id, &config);
		    const KeyedVector<String8, String8>& map(config.getProperties());
		    for (size_t j = 0 ; j < map.size() ; j++) {
			printf("  %s: %s\n", map.keyAt(j).string(), map.valueAt(j).string());
		    }

		    for (int j = 0 ; j < 4 ; j++) {
			if (hub->hasRelativeAxis(id, j)) 
			    printf("  Has relative axis %d\n", j);
		    }

		    for (int j = 0 ; s_abs_axis[j].name ; j++) {
			RawAbsoluteAxisInfo axis_info;
			status_t result = hub->getAbsoluteAxisInfo(id, s_abs_axis[j].axis, &axis_info);
			if (result == NO_ERROR && axis_info.valid)
			    printf("  Has absolute axis %s min=%d max=%d\n", s_abs_axis[j].name,
				   axis_info.minValue, axis_info.maxValue);
		    }
		}

		if (buffer[i].type == EventHubInterface::FINISHED_DEVICE_SCAN) {
		    finished_scan = true;
		}
	    }
	}
    }
}




