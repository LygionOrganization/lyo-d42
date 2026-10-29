#pragma once
#include "Arduino.h"
#include <vector>
struct twai_message_t{uint32_t identifier=0;bool extd=false,rtr=false;uint8_t data_length_code=0,data[8]={};};
inline std::vector<twai_message_t> sent;
inline twai_message_t incoming;inline bool available=false;
#define ESP_OK 0
inline int twai_transmit(const twai_message_t* f,int){sent.push_back(*f);return ESP_OK;}
inline int twai_receive(twai_message_t* f,int){if(!available)return -1;*f=incoming;available=false;return ESP_OK;}
struct Can{int convertSpeed(int v){return v;}bool begin(int,int,int,int,int){return true;}};inline Can ESP32Can;
