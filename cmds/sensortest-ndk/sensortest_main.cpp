#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <android/sensor.h>
#include <android/looper.h>


ASensorEventQueue* sensorEventQueue;
void* sensor_data;

static int get_sensor_events(int fd, int events, void* data) {
  ASensorEvent event;

  printf("Callback. fd: %d, events %d\n", fd, events);

  while (ASensorEventQueue_getEvents(sensorEventQueue, &event, 1) > 0) {
    if(event.type==ASENSOR_TYPE_ACCELEROMETER) {
            printf("accel(x,y,z,t): %f %f %f %lld\n",
                 event.acceleration.x, event.acceleration.y,
                 event.acceleration.z, event.timestamp);
    }
  }
  /* return 1 to continue getting callbacks */
  return 1;
}



int main()
{
	ASensorManager* sensorManager = NULL;
	const ASensor* accelerometerSensor = NULL;
	ASensorList* sensorList = NULL;
	ALooper* looper = NULL;


	if((looper = ALooper_prepare(0)) == NULL)
			printf("Looper NULL\n");

	if((sensorManager = ASensorManager_getInstance()) == NULL)
		printf("sensorManager NULL\n");

	if((accelerometerSensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER)) == NULL)
		printf("accelerometerSensor NULL\n");

	/* get sensor list */
	ASensorList list;

	int num = ASensorManager_getSensorList(ASensorManager_getInstance(), &list);
	int j;
		char* buffer = (char*)calloc(num, 128);



		for (j = 0; j < num; j++) {
			ASensorRef sensor = list[j];
			int type = ASensor_getType(sensor);

			const char* id = ASensor_getName(sensor);
			strcat(buffer,"* ");
			strcat(buffer,id);
			strcat(buffer,"\n");

		}


		printf("Sensors present:\n%s\n",buffer);



	/* LOOPER_ID_USER = 3 */
	if((sensorEventQueue = ASensorManager_createEventQueue(sensorManager,
	            looper, 3, get_sensor_events, sensor_data)) == NULL)
		printf("sensorEventQueue NULL\n");

	int ret = ASensorEventQueue_enableSensor(sensorEventQueue,
	                        accelerometerSensor);
	printf("enableSensor ret: %d\n", ret);

	int rate = ASensor_getMinDelay(accelerometerSensor) * 100;
	printf("poll rate: %d usec\n", rate);
    
	ret = ASensorEventQueue_setEventRate(sensorEventQueue,
	                        accelerometerSensor, rate);
	printf("setEventRate ret: %d\n", ret);


	int ident = 0;
	int events = 0;
	int fd = 0;

	printf("begin accelerometer polling\n");
	ident = ALooper_pollAll(-1, &fd, &events, &sensor_data);

	return 0;
}
