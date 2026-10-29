#pragma once
#include <ArduinoJson.h>
#include <lygion_devs.h>
#include <atomic>
#include <mutex>
#include "ChassisMath.h"

class LyoD42 {
 public:
  void begin();
  void releaseForDebug();
  bool handle(const JsonDocument& in,JsonDocument& out,uint32_t received);
  void snapshot(JsonDocument& out);
  bool idleDisplaySnapshot(float& batteryVoltage);
  void requestStop() { urgentStop_.store(true); }
  uint8_t canId() const { return canId_.load(); }
 private:
  void tick();
  bool torque_[2]={false,false};
  void stop(const char* reason,bool disable=true);
  bool setMode(uint8_t mode);
  bool prepareMotors();
  bool startMotion(uint32_t received);
  bool hold_=false,holdEnabled_=false,debug_=false;
  bool enterHold();
  bool leaveHold();
  bool motorError(int id,const char* reason);
  bool good(int result);
  bool speed(double left,double right,uint8_t acc);
  bool disableOutputs();
  bool finishStop();
  bool rampStep(uint32_t now);
  bool ramp_=false,releasePending_=false;
  double appliedLeft_=0,appliedRight_=0,rampAcceleration_=0.2;
  double positionAcceleration_=0;
  uint32_t rampAt_=0,releaseAt_=0;
  int currentRaw_[2]={0,0};
  bool currentValid_[2]={false,false},currentSampled_=false;
  uint32_t currentReadAt_=0;
  bool nextSegment();
  bool readFeedback();
  bool save();
  std::mutex mutex_;
  TTLSDClass driver_;
  std::atomic<bool> urgentStop_{false};
  std::atomic<uint8_t> canId_{1};
  bool ready_=false,enabled_=false,position_=false,segment_=false,stopSent_=false;
  bool taskRunning_=false;
  int errorId_=0;
  const char* motorError_="";
  bool valid_[2]={false,false};
  uint32_t feedbackAt_[2]={0,0},lastHeartbeat_=0,lastPoll_=0,segmentAt_=0,segmentLimit_=0;
  int pos_[2]={0,0},velocity_[2]={0,0},voltage_[2]={0,0},temperature_[2]={0,0},fault_[2]={0,0},moving_[2]={0,0};
  int target_[2]={0,0},accMultiplier_[2]={0,0};
  double left_=0,right_=0,remaining_=0,segmentDistance_=0,moveSpeed_=0.05;
  int leftSign_=1,rightSign_=1;
  double maxSpeed_=lyo_d42::maxSpeed,light_=0,minDuty_=0.40;
  uint16_t current_=200,timeout_=3000;
  uint8_t acc_=15;
  const char* state_="starting";
};
extern LyoD42 chassis;
