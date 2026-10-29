#include <ArduinoJson.h>
#include <atomic>
#include <mutex>
#include <cmath>
#include <cassert>
#include <iostream>
#include "ChassisMath.h"
#define private public
#include "LyoD42.h"
#undef private
#include "../src/LyoD42.cpp"
bool command(LyoD42& c,const char* text,uint32_t at=testClock){JsonDocument in,out;assert(!deserializeJson(in,text));assert(c.handle(in,out,at));return out["ok"].as<bool>();}
void poll(LyoD42& c,int ms=110){testClock+=ms;c.tick();}
int main(){
 assert(std::abs(lyo_d42::stepsPerMeter-76495.311)<1);
 assert(lyo_d42::speedRegister(lyo_d42::rpmToMps(60))==332);
 assert(lyo_d42::lightDuty(0,.7)==0&&lyo_d42::lightDuty(.0001,.7)>=716&&lyo_d42::lightDuty(1,.7)==1023);
 double l=.2,r=.1;lyo_d42::limit(l,r,.1);assert(l==.1&&r==.05);
 prefsTest::opened.clear();
 prefsTest::currentMissing=true;
 LyoD42 legacy;legacy.begin();
 assert(prefsTest::opened.size()==2 && prefsTest::opened[0]=="lyo-d42" && prefsTest::opened[1]=="lylumo");
 assert(legacy.save() && prefsTest::opened.back()=="lyo-d42");
 prefsTest::currentMissing=false;prefsTest::opened.clear();
 LyoD42 c;c.begin();
 assert(prefsTest::opened.size()==1 && prefsTest::opened[0]=="lyo-d42");assert(c.ready_&&!c.enabled_&&duty==0);
 JsonDocument telemetry;c.snapshot(telemetry);assert(telemetry["wheels"][0]["temperature_c"]==36 && telemetry["wheels"][1]["temperature_c"]==37);
 assert(command(c,"{\"T\":804,\"v\":0.1,\"w\":0}"));
 command(c,"{\"T\":800}");
 assert(command(c,"{\"T\":801,\"enable\":true}"));
 assert(!command(c,"{\"T\":801,\"enable\":true}"));
 assert(command(c,"{\"T\":804,\"v\":0.05,\"w\":0}"));assert(fake::speed[0]>0&&fake::speed[1]<0);
 assert(!command(c,"{\"T\":804,\"v\":\"0.1\",\"w\":0}"));
 assert(!command(c,"{\"T\":804,\"v\":0.1,\"w\":0}",testClock-3000));
 poll(c,3000);assert(!c.enabled_&&std::string(c.state_)=="watchdog");
 int writes=fake::writes;poll(c);poll(c);assert(fake::writes==writes);
 assert(command(c,"{\"T\":801,\"enable\":true}"));
 assert(command(c,"{\"T\":804,\"v\":0,\"w\":1}"));assert(fake::speed[0]<0&&fake::speed[1]<0);
 c.requestStop();poll(c);assert(!c.enabled_);
 assert(command(c,"{\"T\":801,\"enable\":true}"));
 assert(command(c,"{\"T\":805,\"distance\":0.5,\"speed\":0.03,\"acc\":15}"));
 assert(fake::targets[0]==62767&&fake::targets[1]==2767);
 assert(!command(c,"{\"T\":804,\"v\":0.1,\"w\":0}"));
 fake::pos[0]=fake::targets[0];fake::pos[1]=fake::targets[1];fake::moving[0]=fake::moving[1]=0;
 poll(c,150);assert(c.position_&&c.segment_&&fake::targets[0]<62767);
 fake::pos[0]=fake::targets[0];fake::pos[1]=fake::targets[1];fake::moving[0]=fake::moving[1]=0;
 poll(c,150);assert(!c.enabled_&&std::string(c.state_)=="completed");
 assert(command(c,"{\"T\":801,\"enable\":true}"));
 assert(command(c,"{\"T\":806,\"degrees\":90,\"speed\":0.03}"));assert(fake::targets[0]<32767&&fake::targets[1]<32767);
 c.requestStop();poll(c);fake::moving[0]=fake::moving[1]=0;
 assert(command(c,"{\"T\":801,\"enable\":true}"));fake::status[0]=4;poll(c);assert(!c.enabled_);fake::status[0]=16;
 assert(command(c,"{\"T\":801,\"enable\":true}"));fake::failId=2;poll(c);assert(!c.enabled_);fake::failId=0;
 testClock=0xfffffff0U;assert(command(c,"{\"T\":801,\"enable\":true}"));poll(c,3000);assert(!c.enabled_);
 assert(command(c,"{\"T\":807,\"brightness\":0.001}"));assert(duty>=409);
 assert(!command(c,"{\"T\":807,\"brightness\":1.1}"));
 LyoD42 bad;fake::phase=16;bad.begin();assert(bad.ready_&&fake::phase==16);
 assert(bad.errorId_==0);
 assert(command(bad,"{\"T\":803,\"left\":0,\"right\":0.03}"));
 assert(fake::torque[0]==0&&fake::torque[1]==1);
 command(bad,"{\"T\":800}");assert(fake::torque[0]==0&&fake::torque[1]==0);fake::phase=0;
 assert(command(bad,"{\"T\":801,\"enable\":true}")); // explicit retry after fixing configuration
 command(bad,"{\"T\":800}");
 // Only ID1 and ID2 exist in this fake. Every unicast asserts that invariant.
 for(int id=1;id<=2;++id) {
   fake::failId=id; LyoD42 late; late.begin();
   assert(!late.ready_&&!late.enabled_&&late.errorId_==id);
   assert(!command(late,"{\"T\":801,\"enable\":true}"));
   fake::failId=0; poll(late); assert(!late.enabled_); // no automatic resume
   assert(command(late,"{\"T\":801,\"enable\":true}"));
   assert(late.ready_&&late.errorId_==0&&fake::speed[0]==0&&fake::speed[1]==0);
   command(late,"{\"T\":800}");
 }
 fake::multiplier=0; LyoD42 config; config.begin();
 assert(!command(config,"{\"T\":801,\"enable\":true}"));
 assert(std::string(config.motorError_)=="acceleration_multiplier_zero");fake::multiplier=1;
 fake::status[1]=4;assert(!command(config,"{\"T\":801,\"enable\":true}"));
 assert(config.errorId_==2&&std::string(config.motorError_)=="driver_fault");fake::status[1]=16;
 assert(command(config,"{\"T\":801,\"enable\":true}"));command(config,"{\"T\":800}");
 config.requestStop();assert(!command(config,"{\"T\":801,\"enable\":true}"));poll(config);
 fake::ioDelay=350;assert(!command(config,"{\"T\":801,\"enable\":true}"));fake::ioDelay=0;
 assert(!config.enabled_&&fake::torque[0]==0&&fake::torque[1]==0);
 taskResult=0;LyoD42 noTask;noTask.begin();taskResult=1;
 assert(!command(noTask,"{\"T\":801,\"enable\":true}"));
 assert(std::string(noTask.state_)=="task_start_failed");
 assert(command(config,"{\"T\":811,\"hold\":true}"));
 assert(config.hold_&&fake::holdCurrent==60);poll(config,1000);assert(config.enabled_);
 assert(command(config,"{\"T\":804,\"v\":0.03,\"w\":0}"));
 assert(config.holdEnabled_&&!config.hold_&&!config.position_&&fake::speed[0]>0&&fake::speed[1]<0);
 assert(command(config,"{\"T\":811,\"hold\":true}"));assert(!config.hold_&&config.left_>0);
 assert(command(config,"{\"T\":811,\"hold\":false}"));assert(config.enabled_&&!config.holdEnabled_&&config.left_>0);
 assert(command(config,"{\"T\":811,\"hold\":true}"));
 assert(command(config,"{\"T\":804,\"v\":0,\"w\":0}"));
 assert(config.hold_&&config.holdEnabled_&&fake::holdCurrent==60&&fake::torque[0]==1&&fake::torque[1]==1);
 int holdWrites=fake::writes;
 assert(command(config,"{\"T\":804,\"v\":0,\"w\":0}"));assert(fake::writes==holdWrites);
 assert(command(config,"{\"T\":805,\"distance\":0.1,\"speed\":0.03}"));
 assert(!config.hold_&&config.segment_&&fake::holdCurrent==200);
 fake::pos[0]=fake::targets[0];fake::pos[1]=fake::targets[1];fake::moving[0]=fake::moving[1]=0;
 poll(config,150);assert(config.hold_&&fake::holdCurrent==60&&std::string(config.state_)=="completed");

 assert(command(config,"{\"T\":811,\"hold\":false}"));assert(!config.enabled_);
 assert(command(config,"{\"T\":804,\"v\":0.03,\"w\":0}"));
 assert(command(config,"{\"T\":804,\"v\":0,\"w\":0}"));assert(!config.enabled_);
 config.releaseForDebug();assert(config.debug_);assert(command(config,"{\"T\":804,\"v\":0.03,\"w\":0}"));assert(!config.debug_);
 command(config,"{\"T\":800}");
 assert(command(config,"{\"T\":809,\"timeout_ms\":1000}"));assert(config.timeout_==1000);
 assert(!command(config,"{\"T\":809,\"timeout_ms\":99}"));
 assert(!command(config,"{\"T\":809,\"timeout_ms\":60001}"));
 assert(command(config,"{\"T\":811,\"hold\":true}"));
 poll(config,999);assert(config.enabled_);
 assert(command(config,"{\"T\":810}"));poll(config,999);assert(config.enabled_);
 assert(command(config,"{\"T\":808}"));poll(config,1);assert(!config.enabled_);
 assert(lyo_d42::speedRegister(10)==489&&lyo_d42::speedRegister(-10)==-489);
 assert(std::abs(lyo_d42::maxSpeed-0.32)<0.0001);

 LyoD42 ramp;ramp.begin();
 JsonDocument amps;ramp.snapshot(amps);assert(amps["wheels"][0]["current_raw"]==123 && amps["wheels"][1]["current_raw"]==-45);
 assert(!command(ramp,"{\"T\":804,\"v\":0.1,\"w\":0,\"accel_mps2\":0}"));
 assert(!command(ramp,"{\"T\":804,\"v\":0.1,\"w\":0,\"accel_mps2\":0.2,\"acc\":15}"));
 assert(command(ramp,"{\"T\":803,\"left\":0.1,\"right\":0.05,\"accel_mps2\":0.2}"));
 poll(ramp,50);assert(std::abs(ramp.appliedLeft_-.01)<1e-6 && std::abs(ramp.appliedRight_-.005)<1e-6);
 for(int i=0;i<10;++i)poll(ramp,50);assert(std::abs(ramp.appliedLeft_-.1)<1e-6);
 assert(command(ramp,"{\"T\":804,\"v\":0,\"w\":0,\"accel_mps2\":0.2,\"soft_stop\":true}"));
 poll(ramp,50);assert(ramp.enabled_&&ramp.appliedLeft_>.08);
 for(int i=0;i<12;++i)poll(ramp,50);assert(!ramp.enabled_&&fake::torque[0]==0&&fake::torque[1]==0);
 assert(command(ramp,"{\"T\":811,\"hold\":true}"));
 assert(command(ramp,"{\"T\":804,\"v\":0.1,\"w\":0,\"accel_mps2\":0.2}"));
 for(int i=0;i<12;++i)poll(ramp,50);
 assert(command(ramp,"{\"T\":804,\"v\":0,\"w\":0,\"accel_mps2\":0.2}"));
 for(int i=0;i<12;++i)poll(ramp,50);assert(ramp.hold_&&fake::holdCurrent==60);
 fake::reenableOnTarget=true;assert(command(ramp,"{\"T\":811,\"hold\":false}"));
 assert(!ramp.releasePending_&&fake::torque[0]==0&&fake::torque[1]==0);fake::reenableOnTarget=false;
 assert(command(ramp,"{\"T\":804,\"v\":0.1,\"w\":0,\"accel_mps2\":0.2}"));poll(ramp,50);
 fake::ignoreDisable=true;command(ramp,"{\"T\":800}");assert(ramp.releasePending_);
 fake::ignoreDisable=false;poll(ramp,110);assert(!ramp.releasePending_&&fake::torque[0]==0&&fake::torque[1]==0);
 fake::currentUnsupported=true;poll(ramp,600);ramp.snapshot(amps);assert(amps["wheels"][0]["current_valid"]==false);fake::currentUnsupported=false;
 assert(command(ramp,"{\"T\":804,\"v\":0.1,\"w\":0,\"accel_mps2\":0.01}"));poll(ramp,50);
 poll(ramp,3000);assert(!ramp.enabled_&&!ramp.ramp_); // Watchdog bypasses a slow ramp.
 assert(command(ramp,"{\"T\":804,\"v\":0.1,\"w\":0,\"accel_mps2\":0.2}"));poll(ramp,50);
 assert(command(ramp,"{\"T\":804,\"v\":0,\"w\":0,\"accel_mps2\":0.2,\"soft_stop\":false}"));
 assert(!ramp.enabled_&&!ramp.ramp_&&fake::torque[0]==0&&fake::torque[1]==0);

 assert(lyo_d42::speedMps(lyo_d42::speedRegister(1))<=0.32);
 assert(!command(ramp,"{\"T\":809,\"max_speed\":0.321}"));
 std::cout<<"PASS production chassis: units/direction, limits, input validation, disabled startup, watchdog/wrap, stop dedup, segmented position, rotation, faults, missing motor, LED\n";
}
