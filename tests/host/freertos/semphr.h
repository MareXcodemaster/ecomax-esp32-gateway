#pragma once
using SemaphoreHandle_t = int*;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static int mutex; return &mutex; }
inline void xSemaphoreTake(SemaphoreHandle_t, unsigned) {}
inline void xSemaphoreGive(SemaphoreHandle_t) {}
