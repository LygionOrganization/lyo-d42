#pragma once
#include <cstddef>
class Preferences {
 public:bool begin(const char*,bool){return true;}void end(){}
 double getDouble(const char*,double d){return d;}unsigned char getUChar(const char*,unsigned char d){return d;}unsigned short getUShort(const char*,unsigned short d){return d;}
 size_t putDouble(const char*,double){return 8;}size_t putUChar(const char*,unsigned char){return 1;}size_t putUShort(const char*,unsigned short){return 2;}
};
