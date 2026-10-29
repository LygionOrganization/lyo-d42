#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace lyo_d42 {
constexpr double pi=3.14159265358979323846;
constexpr double track=0.159, diameter=0.069, gear=5.1818181818182;
constexpr double stepsPerMeter=3200*gear/(pi*diameter);
constexpr double speedUnit=50; // driver register: 50 microsteps/s
constexpr double maxSpeed=0.32;
constexpr long maxSpeedRegister=long(maxSpeed*stepsPerMeter/speedUnit); // Floor: quantization must not exceed the cap.
inline double rpmToMps(double rpm) { return rpm*pi*diameter/60; }
inline int16_t speedRegister(double mps) { return int16_t(std::clamp(std::lround(mps*stepsPerMeter/speedUnit),-maxSpeedRegister,maxSpeedRegister)); }
inline double speedMps(int16_t reg) { return reg*speedUnit/stepsPerMeter; }
inline void limit(double& left,double& right,double maximum) {
  double peak=std::max(std::abs(left),std::abs(right));
  if(peak>maximum) { left*=maximum/peak; right*=maximum/peak; }
}
inline double turnWheelDistance(double degrees) { return degrees*pi/180*track/2; }
inline uint16_t lightDuty(double level,double minimum) {
  return level<=0?0:uint16_t(std::lround(1023*(minimum+(1-minimum)*level)));
}
inline int decodeSpeed(uint8_t lo,uint8_t hi) {
  unsigned v=unsigned(lo)|(unsigned(hi)<<8); return (v&0x8000)?-int(v&0x7fff):int(v);
}
}
