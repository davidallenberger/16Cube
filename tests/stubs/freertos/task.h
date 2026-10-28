#pragma once
#include "FreeRTOS.h"
inline BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int){return pdPASS;}
inline void vTaskDelay(unsigned){}
