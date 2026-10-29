#include <ArduinoJson.h>
#include <cmath>
#include <cassert>
#include <string>
#include <iostream>
#include "Arduino.h"
using String=std::string;
struct Chassis{int canId(){return 1;}} chassis;
struct Ws{bool availableForWriteAll(){return true;}void textAll(const String&){}} ws;
void msg(const String&){}
String queued;void addJsonCmd2Queue(const char* s){queued=s;}
#include "../src/Can.h"
void receive(uint32_t id,bool ext=true,int length=8){queued.clear();incoming={};incoming.identifier=id;incoming.extd=ext;incoming.data_length_code=length;incoming.data[0]=42;incoming.data[2]=0x9c;incoming.data[3]=0xff;incoming.data[4]=100;incoming.data[6]=15;available=true;receiveCANasJson();}
int main(){
 receive(0x18EF0184);JsonDocument cmd;assert(!deserializeJson(cmd,queued));assert(cmd["T"]==804&&cmd["reliable"]==true);assert(std::abs(cmd["v"].as<double>()+.1)<1e-6);assert(cmd["_can"]==1);
 receive(0x18EF0204);assert(queued.empty());receive(0x18EF0104,false);assert(queued.empty());receive(0x18EF0104,true,7);assert(queued.empty());receive(0x18EF017F);assert(queued.empty());
 receive(0x18EF0100);assert(!sent.empty()&&sent.back().identifier==0x18EE0100&&sent.back().data[0]==42);
 receive(0x18EF010C);assert(!deserializeJson(cmd,queued));assert(cmd["T"]==809 && cmd["timeout_ms"]==65436);
 JsonDocument ack;ack["ok"]=true;lyo_d42CanResult(cmd,ack);assert(sent.back().identifier==0x18EE010C);
 receive(0x18EF0104);incoming.data[6]=20;incoming.data[7]=0x81;available=true;receiveCANasJson();
 assert(!deserializeJson(cmd,queued));assert(std::abs(cmd["accel_mps2"].as<double>()-.2)<1e-6 && cmd["soft_stop"]==true && cmd["acc"].isNull());
 JsonDocument currents;currents["ok"]=true;cmd["T"]=808;currents["wheels"][0]["current_raw"]=123;currents["wheels"][0]["current_valid"]=true;
 lyo_d42CanResult(cmd,currents);assert(sent.back().identifier==0x18ED0102 && sent.back().data[2]==123 && sent.back().data[6]==1);
 uint8_t data[8]={};assert(!sendCANFrame(0x800,false,data,8));assert(!sendCANFrame(1,false,data,9));
 JsonDocument raw;deserializeJson(raw,"{\"id\":1,\"ext\":false,\"data\":[256]}");assert(!sendJsonAsCAN(raw));
 deserializeJson(raw,"{\"id\":1,\"ext\":false,\"data\":[1,255]}");assert(sendJsonAsCAN(raw));
 std::cout<<"PASS production CAN: addressing, reliable bit, signed units, frame lengths, stop receipt, TX validation\n";
}
