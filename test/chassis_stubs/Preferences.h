#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace prefsTest { inline bool currentMissing=false; inline std::vector<std::string> opened; }
class Preferences {
 public:bool begin(const char* name,bool readOnly){prefsTest::opened.push_back(name);return !(readOnly && prefsTest::currentMissing && std::string(name)=="lyo-d42");}void end(){}
 double getDouble(const char*,double d){return d;}unsigned char getUChar(const char*,unsigned char d){return d;}unsigned short getUShort(const char*,unsigned short d){return d;}
 size_t putDouble(const char*,double){return 8;}size_t putUChar(const char*,unsigned char){return 1;}size_t putUShort(const char*,unsigned short){return 2;}
};
