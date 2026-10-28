#pragma once
#include "FreeRTOS.h"
#include <mutex>
using SemaphoreHandle_t=std::mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new std::mutex;}
inline bool xSemaphoreTake(SemaphoreHandle_t m,unsigned){m->lock();return true;}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
