#include <ArduinoJson.h>
#include <cassert>
#include <iostream>
#include "IdleDisplayPolicy.h"
#include "LyoD42.h"
#include "../src/LyoD42.cpp"
#include "../src/ScreenCtrl.cpp"
int main(){
  ScreenCtrl screen;
  for(int failure=0;failure<3;++failure){
    oledAttached=failure!=0;oledAllocates=failure!=1;i2cAvailable=failure!=2;
    screen.init();assert(!screen.present());
    int before=frames;
    screen.clearDisplay();screen.updateFrame();screen.updateScreen();screen.rotate(1);
    screen.displayText("optional",0,0,1);screen.changeSingleLine(1,"ignored",true);
    assert(frames==before);
    LyoD42 base;base.begin();JsonDocument in,out;
    deserializeJson(in,"{\"T\":801,\"enable\":true}");
    assert(base.handle(in,out,millis())&&out["ok"]==true);
    deserializeJson(in,"{\"T\":804,\"v\":0.03,\"w\":0}");
    out.clear();assert(base.handle(in,out,millis())&&out["ok"]==true);
    assert(fake::speed[0]>0&&fake::speed[1]<0);
    deserializeJson(in,"{\"T\":800}");base.handle(in,out,millis());
  }
  oledAttached=oledAllocates=i2cAvailable=true;screen.init();assert(screen.present());
  const int before=frames;screen.changeSingleLine(1,"Lyo-D42",true);assert(frames>before);
  int idleFrames=frames;i2cAvailable=false;
  assert(!screen.tryIdleFrame("Lyo-D42","Battery: 20.0 V","AP:1","STA:2")&&frames==idleFrames);
  i2cAvailable=true;assert(screen.tryIdleFrame("Lyo-D42","Battery: 20.0 V","AP:1","STA:2"));assert(frames==idleFrames+1);
  assert(!idleDisplayDue(60000,0,59500)); // Recent traffic defers even an overdue frame.
  assert(!idleDisplayDue(59999,0,0));assert(idleDisplayDue(60000,0,59000));
  assert(idleDisplayDue(1000,uint32_t(1000-60000),uint32_t(1000-1000))); // Wrap-safe.
  LyoD42 idle;idle.begin();float battery;
  assert(idle.idleDisplaySnapshot(battery)&&battery==20.0f);
  JsonDocument in,out;deserializeJson(in,"{\"T\":804,\"v\":0.03,\"w\":0}");
  idle.handle(in,out,millis());assert(!idle.idleDisplaySnapshot(battery));
  deserializeJson(in,"{\"T\":800}");idle.handle(in,out,millis());
  testClock+=1001;assert(idle.idleDisplaySnapshot(battery)&&battery<0);
  oledAttached=false;screen.init();assert(!screen.present());
  std::cout<<"PASS absent/failed/busy OLED does not prevent chassis enable or motion; healthy OLED renders\n";
}
