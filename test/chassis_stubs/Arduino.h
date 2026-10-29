#pragma once
#include <cstdint>
#include <algorithm>
using u8=uint8_t;using u16=uint16_t;using s16=int16_t;
inline uint32_t testClock=1000;inline int duty=-1;
inline int taskResult=1;
inline uint32_t millis(){return testClock;}
inline void delayMicroseconds(int){}
inline bool ledcAttach(int,int,int){return true;}
inline void ledcWrite(int,uint32_t d){duty=d;}
struct HardwareSerial{void flush(){}};inline HardwareSerial Serial1;
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void*,int){return taskResult;}
inline void vTaskDelay(int){}
