#pragma once
#include "FreeRTOS.h"
#ifdef CUBINA_TEST_TASK_LIFECYCLE
BaseType_t testCreatePinnedTask(void(*)(void*), int);
void testTaskDelay(unsigned);
int xPortGetCoreID();
inline BaseType_t xTaskCreatePinnedToCore(void(*entry)(void*),const char*,unsigned,void*,unsigned,void*,int core){return testCreatePinnedTask(entry,core);}
inline void vTaskDelay(unsigned ticks){testTaskDelay(ticks);}
#else
inline BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int){return pdPASS;}
inline void vTaskDelay(unsigned){}
inline int xPortGetCoreID(){return 0;}
#endif
