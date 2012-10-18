#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <utils/String8.h>

#include <hardware_legacy/power.h>
#include <hardware/hardware.h>
#include <hardware/lights.h>

using namespace android;

class Light {
public:
    Light(hw_module_t *module, const char *name) 
	: mDevice(0)
	, mName(name) {
	hw_device_t *device;
	int err = module->methods->open(module, name, &device);
	    //fprintf(stderr, "Unable to open light device %s, err=%d\n", name, err);
	if (!err)
	    mDevice = (light_device_t *)device;
	    //fprintf(stderr, "Opened light device %s\n", name);
    }

    void setLight(int color, int flashingMode, int onMS, int offMS, int brightnessMode) {
	light_state_t state;
	memset(&state, 0, sizeof(light_state_t));
	state.color          = color;
	state.flashMode      = flashingMode;
	state.flashOnMS      = onMS;
	state.flashOffMS     = offMS;
	state.brightnessMode = brightnessMode;
	mDevice->set_light(mDevice, &state);
    }

    bool        alive() const { return mDevice != 0; }
    const char *name() const { return mName.string(); }

private:
    light_device_t *mDevice;
    String8         mName;
};

// -------------------------------------------------------------------

class Lights {
public:
    enum BrightnessMode { BRIGHTNESS_USER, BRIGHTNESS_SENSOR };
    enum FlashingMode { FLASH_NONE, FLASH_TIMED, FLASH_HARDWARE };
    enum { MAX_LIGHT_COUNT = 8 };

    Lights() {
	memset(mLights, 0, sizeof(mLights));

	hw_module_t* module;
	int err = hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
	if (err == 0) {
	    int i = 0;
	    mLights[i++] = new Light(module, LIGHT_ID_BACKLIGHT);
	    mLights[i++] = new Light(module, LIGHT_ID_KEYBOARD);
	    mLights[i++] = new Light(module, LIGHT_ID_BUTTONS);
	    mLights[i++] = new Light(module, LIGHT_ID_BATTERY);
	    mLights[i++] = new Light(module, LIGHT_ID_NOTIFICATIONS);
	    mLights[i++] = new Light(module, LIGHT_ID_ATTENTION);
	    mLights[i++] = new Light(module, LIGHT_ID_BLUETOOTH);
	    mLights[i++] = new Light(module, LIGHT_ID_WIFI);
	}
    }

    void setBrightness( int index, int brightness ) {
	int color = brightness & 0x000000ff;
	color = 0xff000000  | (color << 16) | (color << 8) | color;
	setLight(index, color, FLASH_NONE, 0, 0, BRIGHTNESS_USER);
    }

    void setColor( int index, int color ) {
	setLight(index, color, FLASH_NONE, 0, 0, BRIGHTNESS_USER);
    }

    void setFlashing( int index, int color, int onMS, int offMS ) {
	setLight(index, color, FLASH_HARDWARE, onMS, offMS, BRIGHTNESS_USER);
    }

    void setLight( int index, int colorARGB, FlashingMode flashingMode, 
			   int onMS, int offMS, BrightnessMode brightnessMode ) {
	if (index >= 0 && index < MAX_LIGHT_COUNT && mLights[index])
	    mLights[index]->setLight(colorARGB, flashingMode, onMS, offMS, brightnessMode);
    }

    int nameToIndex(const char *name) {
	for (int i = 0 ; i < MAX_LIGHT_COUNT ; i++) 
	    if (mLights[i] && strncmp(mLights[i]->name(), name, 3) == 0)
		return i;
	return -1;
    }
    
    void dump() {
	for (int i = 0 ; i < MAX_LIGHT_COUNT ; i++)
	    if (mLights[i] && mLights[i]->alive())
		printf("%s\n", mLights[i]->name());
    }

private:
    Light *mLights[MAX_LIGHT_COUNT];
};



// -------------------------------------------------------------------

static void usage()
{
    printf("Usage:  lights [CMD...]\n"
	   "\n"
	   "Passing no command shows the available lights.\n"
	   "Other options:\n"
	   "    NAME VALUE              Set light NAME to integer brightness VALUE\n"
	   "    NAME RED GREEN BLUE     Set light NAME to color triple (integer values)\n"
	   "    NAME flash [ON] [OFF]   Flash light ON (milliseconds) and OFF (milliseconds)\n"
	   "\n"
	);
    exit(0);
}

const char * lightName[] = { "backlight", "keyboard", "buttons", "battery",
			     "notifications", "attention", "bluetooth", "wifi" };

int main(int argc, char **argv)
{
    bool loop = false;
    Lights lights;

    if (argc < 2) {
	lights.dump();
    } else {
	int index = lights.nameToIndex(argv[1]);
	if (index < 0)
	    usage();
	else if (argc >= 3 && argc <= 5 && strcmp(argv[2], "flash") == 0) {
	    int ontime  = ( argc >= 4 ? atoi(argv[3]) : 1000);
	    int offtime = ( argc >= 5 ? atoi(argv[4]) : 1000);
	    printf("Starting to flash\n");
	    lights.setFlashing(index, 200, ontime, offtime);
	    loop = true;
	}
	else if (argc == 3) {
	    int level = atoi(argv[2]);
	    lights.setBrightness(index, level);
	}
	else if (argc == 5) {
	    int red = atoi(argv[2]);
	    int green = atoi(argv[3]);
	    int blue = atoi(argv[4]);
	    lights.setColor(index, (0xff000000 | (red << 16) | (green << 8) | blue));
	}
	else 
	    usage();
    }

    while (loop)
	sleep(1);
    return 0;
}




