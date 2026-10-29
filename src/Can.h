#pragma once
#include <ESP32-TWAI-CAN.hpp>
#include "IdleDisplayPolicy.h"
// Extended IDs: 0x18EFnnop request, 0x18EEnnop result; op bit7=reliable.
bool sendCANFrame(uint32_t id,bool ext,const uint8_t* data,uint8_t len) {
  if(len>8||id>(ext?0x1fffffff:0x7ff))return false;
  twai_message_t f={};f.identifier=id;f.extd=ext;f.data_length_code=len;
  memcpy(f.data,data,len);return twai_transmit(&f,0)==ESP_OK;
}
bool sendJsonAsCAN(const JsonDocument& d) {
  if(!d["id"].is<uint32_t>()||(!d["ext"].is<bool>()&&!d["ext"].is<int>()))return false;
  JsonArrayConst a=d["data"];if(a.isNull()||a.size()>8)return false;
  uint8_t bytes[8]={};int i=0;
  for(JsonVariantConst v:a){if(!v.is<int>()||v.as<int>()<0||v.as<int>()>255)return false;bytes[i++]=v.as<int>();}
  return sendCANFrame(d["id"],d["ext"].as<bool>(),bytes,i);
}
void lyo_d42CanResult(const JsonDocument& in,const JsonDocument& out) {
  if(!in["_can"].is<int>())return;
  uint16_t seq=uint16_t(in["rid"].as<uint32_t>());uint32_t node=uint32_t(in["_can"].as<int>())<<8;
  uint8_t d[8]={uint8_t(seq),uint8_t(seq>>8),uint8_t(out["ok"].as<bool>()?0:1)};
  sendCANFrame(0x18EE0000U|node|uint32_t(in["_can_op"] | (in["T"].as<int>()-800)),true,d,3);
  if(in["T"].as<int>()==808) {
    d[2]=out["ready"].as<bool>();d[3]=out["enabled"].as<bool>();
    d[4]=out["wheels"][0]["valid"].as<bool>();d[5]=out["wheels"][1]["valid"].as<bool>();
    d[6]=uint8_t(std::lround(out["wheels"][0]["voltage"].as<double>()*10));d[7]=uint8_t(std::lround(out["led"].as<double>()*100));
    sendCANFrame(0x18ED0000U|node,true,d,8);
    int16_t l=std::lround(out["wheels"][0]["mps"].as<double>()*1000),r=std::lround(out["wheels"][1]["mps"].as<double>()*1000);
    d[2]=uint8_t(l);d[3]=uint8_t(uint16_t(l)>>8);d[4]=uint8_t(r);d[5]=uint8_t(uint16_t(r)>>8);
    d[6]=out["wheels"][0]["status"]|0;d[7]=out["wheels"][1]["status"]|0;
    sendCANFrame(0x18ED0001U|node,true,d,8);
    int16_t c0=out["wheels"][0]["current_raw"]|0,c1=out["wheels"][1]["current_raw"]|0;
    d[2]=uint8_t(c0);d[3]=uint8_t(uint16_t(c0)>>8);d[4]=uint8_t(c1);d[5]=uint8_t(uint16_t(c1)>>8);
    d[6]=out["wheels"][0]["current_valid"]|false;d[7]=out["wheels"][1]["current_valid"]|false;
    sendCANFrame(0x18ED0002U|node,true,d,8);
  }
}
void receiveCANasJson() {
  twai_message_t f={};if(twai_receive(&f,0)!=ESP_OK||f.rtr||f.data_length_code>8)return;
  lastControlActivity.store(millis());
  static uint32_t debugAt=0;
  if(uint32_t(millis()-debugAt)>=50) {
    debugAt=millis();JsonDocument log;log["T"]=501;log["direction"]="rx";log["id"]=f.identifier;log["ext"]=bool(f.extd);
    for(int i=0;i<f.data_length_code;++i)log["data"].add(f.data[i]);
    String text;serializeJson(log,text);if(ws.availableForWriteAll())ws.textAll(text);
  }
  if(!f.extd||(f.identifier&0x1FFFFF00U)!=(0x18EF0000U|(uint32_t(chassis.canId())<<8))||f.data_length_code!=8)return;
  uint8_t op=f.identifier&0x7f;if(op>12)return;
  auto s16=[&](int i){return int16_t(uint16_t(f.data[i])|(uint16_t(f.data[i+1])<<8));};
  JsonDocument cmd;cmd["T"]=op==12?809:800+op;cmd["_can_op"]=op;cmd["rid"]=uint32_t(0xCA000000U|uint16_t(f.data[0]|(f.data[1]<<8)));cmd["_can"]=chassis.canId();cmd["reliable"]=bool(f.identifier&0x80);
  if(op==12)cmd["timeout_ms"]=uint16_t(f.data[2]|(f.data[3]<<8));
  if(op==11){if(f.data[2]>1)return;cmd["hold"]=bool(f.data[2]);}
  if(op==1){if(f.data[2]>1)return;cmd["enable"]=bool(f.data[2]);}
  if(op==2||op==3){cmd["left"]=s16(2)/(op==2?100.0:1000.0);cmd["right"]=s16(4)/(op==2?100.0:1000.0);}
  if(op==4){cmd["v"]=s16(2)/1000.0;cmd["w"]=s16(4)/1000.0;}
  if(op==5||op==6){cmd[op==5?"distance":"degrees"]=s16(2)/(op==5?1000.0:100.0);cmd["speed"]=s16(4)/1000.0;}
  if(op>=2&&op<=6) {
    if(f.data[7]&0x80) {
      if(f.data[7]&0x7e)return;
      cmd["accel_mps2"]=f.data[6]/100.0;
      cmd["soft_stop"]=bool(f.data[7]&1);
    } else cmd["acc"]=f.data[6];
  }
  if(op==7)cmd["brightness"]=uint16_t(f.data[2]|(f.data[3]<<8))/1000.0;
  if(op==9){cmd["can_id"]=f.data[2];cmd["max_speed"]=f.data[3]/500.0;cmd["min_duty"]=f.data[4]/100.0;cmd["timeout_ms"]=uint16_t(f.data[5])*10;cmd["current"]=uint16_t(f.data[6]|(f.data[7]<<8));}
  char text[256];if(serializeJson(cmd,text,sizeof(text))>=255)return;addJsonCmd2Queue(text);
  if(op==0){JsonDocument ack;ack["ok"]=true;lyo_d42CanResult(cmd,ack);}
}
void CanStart() {
  if(!ESP32Can.begin(ESP32Can.convertSpeed(500),14,13,16,16)){msg("CAN start failed");return;}
  xTaskCreatePinnedToCore([](void*){for(;;){receiveCANasJson();vTaskDelay(pdMS_TO_TICKS(2));}},"Lyo-D42CAN",4096,nullptr,2,nullptr,0);
}
