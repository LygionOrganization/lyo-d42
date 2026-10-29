#pragma once
#include "Arduino.h"
#include <cstring>
#include <cassert>
namespace fake {
inline int pos[2]={32767,32767},speed[2]={0,0},status[2]={16,16},moving[2]={0,0},targets[2]={0,0};
inline int phase=0,multiplier=1,failId=0,writes=0,mode[2]={1,1},torque[2]={0,0};
inline int ioDelay=0,holdCurrent=0;
inline bool reenableOnTarget=false,ignoreDisable=false,currentUnsupported=false;
}
class TTLSDClass {
 int error=0;
 int result(int id){assert(id==1||id==2);testClock+=fake::ioDelay;error=id==fake::failId?1:0;return error?0:1;}
 public:
 int writeByte(int id,int reg,int value){if(!result(id))return 0;if(reg==18)fake::phase=value;return 1;}
 HardwareSerial* pSerial=nullptr;int IOTimeOut=0;
 int getLastError(){return error;}
 int EnableTorque(int id,int value){if(!result(id))return 0;if(value || !fake::ignoreDisable)fake::torque[id-1]=value;return 1;}
 int LockEprom(int id){return result(id);}
 int SetMode(int id,int value){if(!result(id))return 0;fake::mode[id-1]=value;return 1;}
 int CalibrationOfs(int id,int value){if(!result(id))return 0;fake::pos[id-1]=value;return 1;}
 int readByte(int id,int reg){if(!result(id))return -1;return reg==40?fake::torque[id-1]:(reg==18?fake::phase:fake::multiplier);}
 int ReadCurrent(int id){if(fake::currentUnsupported){error=1;return -1;}return result(id)?(id==1?123:-45):-1;}
 int Read(int id,int,unsigned char* data,int n){if(!result(id))return 0;std::memset(data,0,n);int i=id-1;data[0]=fake::pos[i]&255;data[1]=fake::pos[i]>>8;int s=fake::speed[i];if(s<0)s=(-s)|0x8000;data[2]=s&255;data[3]=s>>8;data[6]=200;data[7]=36+i;data[9]=fake::status[i];data[10]=fake::moving[i];return n;}
 void SyncWriteSpe(u8*,u8,s16* speeds,u8*,u16*){++fake::writes;if(fake::reenableOnTarget)fake::torque[0]=fake::torque[1]=1;fake::speed[0]=speeds[0];fake::speed[1]=speeds[1];}
 void SyncWritePosEx(u8*,u8,u16* pos,u16*,u8*,u16* current){fake::holdCurrent=current[0];++fake::writes;for(int i=0;i<2;++i){fake::targets[i]=pos[i];fake::moving[i]=1;}}
};
