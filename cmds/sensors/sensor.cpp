/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define SENSOR_EVENT_DEBUG


#include <sensors/sensor.h>

int main(int argc, char **argv)
{
    int opt;
    int cnt=0;
   	int i=0;
    android::KlaatuSensor *k   = new android::KlaatuSensor;

    k->sensor_type = 0;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            cnt++;
            printf("arg %d is %s\n", cnt, optarg);
            while (android::klaatuSensors[i].name != NULL) {
                if (!strcasecmp(optarg, android::klaatuSensors[i].name))
                {
                    k->sensor_type |= android::klaatuSensors[i].type;
                    break;
                }
                i++;
            }
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-t sensor] ...\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    //k->msehf = NULL;
    k->sensor_event_handler = NULL;
    k->initSensor(k->sensor_type);
    
    
    while(1)
        sleep(100);
    printf("%s Exiting.... \n", __FUNCTION__);
    return 0;
}

