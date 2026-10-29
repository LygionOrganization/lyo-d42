#pragma once
#include "Arduino.h"
#include <cassert>
inline bool oledAttached=false,oledAllocates=true;
inline int frames=0,displayBegins=0,probes=0;
struct TwoWire {
  void beginTransmission(int address){assert(address==0x3c);++probes;}
  int endTransmission(){return oledAttached?0:2;}
};
inline TwoWire Wire;
constexpr int SSD1306_SWITCHCAPVCC=2,SSD1306_WHITE=1;
struct Adafruit_SSD1306 {
  Adafruit_SSD1306(int,int,TwoWire*,int,unsigned long,unsigned long){}
  bool begin(int,int,bool,bool){++displayBegins;return oledAllocates;}
  void setTextColor(int){}
  void setTextWrap(bool wrap){assert(!wrap);}
  void clearDisplay(){}
  void display(){++frames;}
  void setRotation(int){}
  void setCursor(int,int){}
  void setTextSize(int){}
  void print(const String&){}
};
