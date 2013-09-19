#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <unistd.h>
#include <android/sensor.h>
#include <android/looper.h>

/* globals */
ASensorEventQueue* g_sensorEventQueue;
void* g_sensor_data;
short g_opts;

/* sensor event callback */
static int get_sensor_events(int fd, int events, void* data) {
  ASensorEvent event;


  while (ASensorEventQueue_getEvents(g_sensorEventQueue, &event, 1) > 0) {
    if(event.type==ASENSOR_TYPE_ACCELEROMETER && (g_opts & (1 << ASENSOR_TYPE_ACCELEROMETER))) {
            printf("accel(x,y,z,t): %f %f %f %lld\n",
                 event.acceleration.x, event.acceleration.y,
                 event.acceleration.z, event.timestamp);
    }
    else if(event.type==ASENSOR_TYPE_MAGNETIC_FIELD && (g_opts & (1 << ASENSOR_TYPE_MAGNETIC_FIELD))) {
        printf("magnetic(x,y,z,t): %f %f %f %lld\n",
             event.magnetic.x, event.magnetic.y,
             event.magnetic.z, event.timestamp);
    }
    else if(event.type==ASENSOR_TYPE_GYROSCOPE && (g_opts & (1 << ASENSOR_TYPE_GYROSCOPE))) {
            printf("gyro(x,y,z,t): %f %f %f %lld\n",
                 event.vector.x, event.vector.y,
                 event.vector.z, event.timestamp);
        }
    else if(event.type==ASENSOR_TYPE_LIGHT && (g_opts & (1 << ASENSOR_TYPE_LIGHT))) {
            printf("light(l,t): %f %lld\n",
                 event.light, event.timestamp);
        }
    else if(event.type==ASENSOR_TYPE_PROXIMITY && (g_opts & (1 << ASENSOR_TYPE_PROXIMITY))) {
            printf("proximity(distance,t): %f %lld\n",
                 event.distance, event.timestamp);
        }
  }
  fflush(stdout);
  /* return 1 to continue getting callbacks */
  return 1;
}



int main(int argc, char *argv[])
{

	int opt = 0;
	g_opts = 0;

	if(argc == 1)
	{
		printf("Missing option parameter. Please run %.25s -h for more info\n", argv[0]);
		return 0;
	}

	while ((opt = getopt(argc, argv, "amglph")) != -1) {
	    switch(opt) {
	    case 'a':
	    	g_opts |= (1 << ASENSOR_TYPE_ACCELEROMETER);
	    	break;
	    case 'm':
	    	g_opts |= (1 << ASENSOR_TYPE_MAGNETIC_FIELD);
	    break;
	    case 'g':
	    	g_opts |= (1 << ASENSOR_TYPE_GYROSCOPE);
	    	break;
	    case 'l':
	    	g_opts |= (1 << ASENSOR_TYPE_LIGHT);
	    	break;
	    case 'p':
	    	g_opts |= (1 << ASENSOR_TYPE_PROXIMITY);
	    	break;

	    case '?':
	    case 'h':
	     printf("\nAvailable options for sensor testing:\n -a: accelerometer\n -m: magnetic field\n -g: gyroscope\n -l: light\n -p: proximity\n You may combine options to test multiple sensors at once.\n");
	     return 0;

	  }
	}


	/* prepare sensors */
	ASensorManager* sensorManager = NULL;
	ASensorList* sensorList = NULL;
	ALooper* looper = NULL;

	const ASensor* accelerometerSensor = NULL;
	const ASensor* magnetometerSensor = NULL;
	const ASensor* gyroscopeSensor = NULL;
	const ASensor* lightSensor = NULL;
	const ASensor* proximitySensor = NULL;

	/* set up looper and get default sensors */
	if((looper = ALooper_prepare(0)) == NULL)
			printf("Looper NULL\n");

	if((sensorManager = ASensorManager_getInstance()) == NULL)
		printf("sensorManager NULL\n");

	if((accelerometerSensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER)) == NULL)
		printf("accelerometerSensor NULL\n");

	if((magnetometerSensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_MAGNETIC_FIELD)) == NULL)
			printf("magnetometerSensor NULL\n");

	if((gyroscopeSensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_GYROSCOPE)) == NULL)
				printf("gyroscopeSensor NULL\n");

	if((lightSensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_LIGHT)) == NULL)
				printf("lightSensor NULL\n");

	if((proximitySensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_PROXIMITY)) == NULL)
				printf("proximitySensor NULL\n");


	/* get device sensor list */
	ASensorList list;

	int num = ASensorManager_getSensorList(ASensorManager_getInstance(), &list);

	char* buffer = (char*)calloc(num, 128);

	for (int i = 0; i < num; i++) {
			ASensorRef sensor = list[i];
			int type = ASensor_getType(sensor);

			const char* id = ASensor_getName(sensor);
			strcat(buffer,"* ");
			strcat(buffer,id);
			strcat(buffer,"\n");

	}

	printf("Sensors present:\n%s\n",buffer);


	/* create event queue */
	/* LOOPER_ID_USER = 3 */
	if((g_sensorEventQueue = ASensorManager_createEventQueue(sensorManager,
	            looper, 3, get_sensor_events, g_sensor_data)) == NULL)
		printf("sensorEventQueue NULL\n");


	/* enable sensors and set polling rate (1 sec = 1000000 microseconds)*/
	/* quickest supported rate can be found using ASensor_getMinDelay() */
	int rate = 1000000;
	int ret = 0;

	if(g_opts & (1 << ASENSOR_TYPE_ACCELEROMETER)){
		ret = ASensorEventQueue_enableSensor(g_sensorEventQueue,
				accelerometerSensor);
		printf("enable accelerometerSensor ret: %d\n", ret);
		ret = ASensorEventQueue_setEventRate(g_sensorEventQueue,
				accelerometerSensor, rate);
		printf("set accelerometerSensor Rate ret: %d\n", ret);
	}

	if(g_opts & (1 << ASENSOR_TYPE_MAGNETIC_FIELD)){
		ret = ASensorEventQueue_enableSensor(g_sensorEventQueue,
				magnetometerSensor);
		printf("enable magnetometerSensor ret: %d\n", ret);
		ret = ASensorEventQueue_setEventRate(g_sensorEventQueue,
				magnetometerSensor, rate);
		printf("set magnetometerSensor Rate ret: %d\n", ret);
	}

	if(g_opts & (1 << ASENSOR_TYPE_GYROSCOPE)){
		ret = ASensorEventQueue_enableSensor(g_sensorEventQueue,
				gyroscopeSensor);
		printf("enable gyroscopeSensor ret: %d\n", ret);
		ret = ASensorEventQueue_setEventRate(g_sensorEventQueue,
				gyroscopeSensor, rate);
		printf("set gyroscopeSensor Rate ret: %d\n", ret);
	}

	if(g_opts & (1 << ASENSOR_TYPE_LIGHT)){
		ret = ASensorEventQueue_enableSensor(g_sensorEventQueue,
				lightSensor);
		printf("enable lightSensor ret: %d\n", ret);
		ret = ASensorEventQueue_setEventRate(g_sensorEventQueue,
				lightSensor, rate);
		printf("set lightSensor Rate ret: %d\n", ret);
	}

	if(g_opts & (1 << ASENSOR_TYPE_PROXIMITY)){
		ret = ASensorEventQueue_enableSensor(g_sensorEventQueue,
				proximitySensor);
		printf("enable proximitySensor ret: %d\n", ret);
		ret = ASensorEventQueue_setEventRate(g_sensorEventQueue,
				proximitySensor, rate);
		printf("set proximitySensor Rate ret: %d\n", ret);

	}
    

	int ident = 0;
	int events = 0;
	int fd = 0;

	printf("begin sensor polling\n");
	ident = ALooper_pollAll(-1, &fd, &events, &g_sensor_data);

	return 0;
}
