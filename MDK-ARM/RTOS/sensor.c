#include "stm32f1xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

#include "driver_timer.h"
#include "driver_dht11.h"

typedef struct
{
	int hum;
	int temp;
}sensor_data_t ;

int count=0;

void Sensor_Task(void *argument)
 {
     DHT11_Init();
    sensor_data_t s1;
    s1.hum = 0;
    s1.temp = 0;
    TickType_t lastWake = xTaskGetTickCount();   /* 记下当前 Tick */



while(1)
    { 
     if(DHT11_Read(&s1.hum,&s1.temp)==0)
{
    count++;
}
else
{
  
}
        DHT11_Update();
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000));
    }   


 }

