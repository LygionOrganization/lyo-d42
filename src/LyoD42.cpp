#include "LyoD42.h"
#include "BusGuard.h"
#include <Preferences.h>
#include <WiFi.h>

LyoD42 chassis;
namespace {
void turnaround() { Serial1.flush(); delayMicroseconds(2000); }
bool number(const JsonDocument& d,const char* key,double low,double high) {
  return d[key].is<double>() && std::isfinite(d[key].as<double>()) && d[key].as<double>()>=low && d[key].as<double>()<=high;
}
bool integer(const JsonDocument& d,const char* key,int low,int high) {
  return d[key].is<int>() && d[key].as<int>()>=low && d[key].as<int>()<=high;
}
}
bool LyoD42::good(int result) { return result>0 && driver_.getLastError()==0; }

void LyoD42::begin() {
  ledcAttach(21,20000,10); ledcWrite(21,0);
  driver_.pSerial=&Serial1; driver_.IOTimeOut=15;
  // Keep the legacy NVS namespace so a product rename preserves saved settings.
  Preferences p;
  if(p.begin("lylumo",true)) {
    double m=p.getDouble("max_speed_v2",lyo_d42::maxSpeed),d=p.getDouble("min_duty_v2",0.40);
    if(std::isfinite(m)&&m>=0.01&&m<=lyo_d42::maxSpeed) maxSpeed_=m;
    if(std::isfinite(d)&&d>=0.01&&d<=1) minDuty_=d;
    int id=p.getUChar("can_id",1); if(id>=1&&id<=127) canId_=id;
    int t=p.getUShort("timeout_v2",3000); if(t>=100&&t<=60000) timeout_=t;
    int c=p.getUShort("current",200); if(c>=1&&c<=1000) current_=c;
    p.end();
  }
  {
    BusGuard bus;
    ready_=prepareMotors();
    stop(ready_?"disabled":"initialization_failed");
  }
  taskRunning_=xTaskCreatePinnedToCore([](void* arg){
    auto* self=static_cast<LyoD42*>(arg);
    for(;;) { self->tick(); vTaskDelay(pdMS_TO_TICKS(10)); }
  },"Lyo-D42",6144,this,3,nullptr,1)==pdPASS;
  if(!taskRunning_) {
    ready_=false; state_="task_start_failed";
  }
}

bool LyoD42::motorError(int id,const char* reason) {
  // Keep the first failure so cleanup cannot hide the original diagnosis.
  if(!errorId_) { errorId_=id; motorError_=reason; }
  return false;
}
bool LyoD42::prepareMotors() {
  errorId_=0; motorError_="";
  // The product has exactly two required bus devices. Never scan or require a Node.
  if(!setMode(1)) return false;
  for(int i=0;i<2;++i) {
    turnaround(); accMultiplier_[i]=driver_.readByte(i+1,86);
    if(driver_.getLastError()!=0 || accMultiplier_[i]<0) return motorError(i+1,"acceleration_read_failed");
    if(accMultiplier_[i]==0) return motorError(i+1,"acceleration_multiplier_zero");
  }
  return readFeedback();
}
bool LyoD42::setMode(uint8_t mode) {
  bool ok=true;
  for(int i=1;i<=2;++i) {
    turnaround(); if(!good(driver_.EnableTorque(i,0))) { ok=motorError(i,"disable_ack_failed"); continue; }
    torque_[i-1]=false;
    // Lock before changing mode: runtime changes must not wear EEPROM.
    turnaround(); if(!good(driver_.LockEprom(i))) { ok=motorError(i,"lock_ack_failed"); continue; }
    turnaround(); if(!good(driver_.SetMode(i,mode))) ok=motorError(i,"mode_ack_failed");
  }
  return ok;
}
bool LyoD42::speed(double left,double right,uint8_t acceleration) {
  if(left==0 && right==0 && stopSent_) return true;
  u8 ids[]={1,2},a[]={acceleration,acceleration};
  s16 v[]={lyo_d42::speedRegister(left),s16(-lyo_d42::speedRegister(right))};
  u16 c[]={uint16_t(v[0]?current_:0),uint16_t(v[1]?current_:0)};
  // A driver may interpret velocity 0 as maximum speed. Disable each zero
  // wheel before transmitting targets; only enable wheels with nonzero targets.
  for(int i=0;i<2;++i) if(v[i]==0 && torque_[i]) {
    turnaround(); if(!good(driver_.EnableTorque(i+1,0))) return motorError(i+1,"disable_ack_failed");
    torque_[i]=false;
  }
  const bool active[]={v[0]!=0,v[1]!=0};
  turnaround(); driver_.SyncWriteSpe(ids,2,v,a,c);
  for(int i=0;i<2;++i) if(active[i] && !torque_[i]) {
    turnaround(); if(!good(driver_.EnableTorque(i+1,1))) return motorError(i+1,"enable_failed");
    torque_[i]=true;
  }
  for(int i=0;i<2;++i) if(!active[i]) {
    turnaround();if(!good(driver_.EnableTorque(i+1,0))) return motorError(i+1,"disable_ack_failed");
    torque_[i]=false;
  }
  appliedLeft_=left;appliedRight_=right;
  stopSent_=left==0 && right==0;
  return true;
}
bool LyoD42::disableOutputs() {
  bool ok=true;
  for(int i=0;i<2;++i) {
    turnaround();bool ack=good(driver_.EnableTorque(i+1,0));
    turnaround();int actual=driver_.readByte(i+1,40);
    if(!ack || driver_.getLastError()!=0 || actual!=0) {ok=false;motorError(i+1,"release_not_confirmed");}
    else torque_[i]=false;
  }
  releasePending_=!ok;releaseAt_=millis();return ok;
}
bool LyoD42::finishStop() {
  ramp_=false;appliedLeft_=appliedRight_=0;
  if(holdEnabled_) return enterHold();
  stop("stopped");return !releasePending_;
}
bool LyoD42::rampStep(uint32_t now) {
  if(!ramp_) return true;
  // Cap catch-up after bus stalls: never make a large instantaneous speed jump.
  double dt=std::min(uint32_t(now-rampAt_),uint32_t(50))/1000.0;
  if(dt<0.01) return true;
  rampAt_=now;
  double dl=left_-appliedLeft_,dr=right_-appliedRight_;
  double peak=std::max(std::abs(dl),std::abs(dr));
  double scale=peak>0?std::min(1.0,rampAcceleration_*dt/peak):1.0;
  double l=appliedLeft_+dl*scale,r=appliedRight_+dr*scale;
  if(scale==1 && left_==0 && right_==0) return finishStop();
  // The ESP32 supplies the ramp; native acceleration=0 avoids a second ramp.
  if(!speed(l,r,0)) {stop("motion_io_failed");return false;}
  if(scale==1) ramp_=false;
  return true;
}
void LyoD42::stop(const char* reason,bool disable) {
  // Position mode must be cancelled before a zero-speed command can be used.
  if(position_) { if(!setMode(1)) ready_=false; position_=segment_=false; }
  // Disable first even if a previous diagnostic command changed zero semantics.
  for(int id=1;id<=2;++id) {turnaround();driver_.EnableTorque(id,0);torque_[id-1]=false;}
  ramp_=false;
  speed(0,0,50);
  disableOutputs(); // Final write is disable, and readback must confirm it.
  appliedLeft_=appliedRight_=0;left_=right_=remaining_=0;
  if(disable) {
    enabled_=false;
  }
  hold_=false; holdEnabled_=false; state_=reason;
}
// The switch is a parking policy; hold_ describes actual position holding.
bool LyoD42::enterHold() {
  ramp_=false;
  if(hold_) return true; // Repeated zero targets must not recapture the position.
  if(!setMode(0)) { stop("hold_failed"); return false; }
  position_=true;
  if(!readFeedback()) { stop("hold_failed"); return false; }
  u8 ids[]={1,2},a[]={50,50};
  u16 targets[]={u16(pos_[0]),u16(pos_[1])},speeds[]={1,1},currents[]={60,60};
  turnaround(); driver_.SyncWritePosEx(ids,2,targets,speeds,a,currents);
  for(int id=1;id<=2;++id) {
    turnaround();
    if(!good(driver_.EnableTorque(id,1))) { motorError(id,"hold_enable_failed");stop("hold_failed");return false; }
    torque_[id-1]=true;
  }
  appliedLeft_=appliedRight_=0;left_=right_=remaining_=0;
  hold_=true;enabled_=true;segment_=false;stopSent_=false;state_="hill_hold";
  return true;
}
bool LyoD42::leaveHold() {
  if(!hold_) return true;
  if(!setMode(1)) { stop("mode_failed");return false; }
  hold_=false;position_=false;stopSent_=false;
  return true;
}
bool LyoD42::readFeedback() {
  bool ok=true;
  const bool pollCurrent=!currentSampled_ || uint32_t(millis()-currentReadAt_)>=500;
  for(int i=0;i<2;++i) {
    uint8_t data[12]={}; turnaround();
    bool valid=driver_.Read(i+1,56,data,sizeof(data))==sizeof(data) && driver_.getLastError()==0;
    valid_[i]=valid;
    if(!valid) { ok=motorError(i+1,"feedback_read_failed"); continue; }
    pos_[i]=data[0]|(data[1]<<8); velocity_[i]=lyo_d42::decodeSpeed(data[2],data[3])*(i==0?1:-1);
    voltage_[i]=data[6]; temperature_[i]=data[7]; fault_[i]=data[9]; moving_[i]=data[10]; feedbackAt_[i]=millis();
    // Current register support varies: a failed optional read must not stop motion.
    if(pollCurrent) {
      turnaround();currentRaw_[i]=driver_.ReadCurrent(i+1);
      currentValid_[i]=driver_.getLastError()==0;
    }
    if(fault_[i]&0xEF) ok=motorError(i+1,"driver_fault"); // bit4 is normal automatic current limiting.
  }
  if(pollCurrent) {currentReadAt_=millis();currentSampled_=true;}
  return ok;
}
bool LyoD42::nextSegment() {
  // Native 16-bit absolute position targets. Rebase only after both wheels stop.
  const int steps=std::min(30000,int(std::llround(remaining_*lyo_d42::stepsPerMeter)));
  if(steps<=0) {
    if(holdEnabled_) { if(!enterHold()) return false;state_="completed"; }
    else stop("completed");
    return true;
  }
  segmentDistance_=steps/lyo_d42::stepsPerMeter;
  for(int i=0;i<2;++i) {
    turnaround(); if(!good(driver_.CalibrationOfs(i+1,32767))) return false;
  }
  target_[0]=32767+steps*leftSign_;
  target_[1]=32767-steps*rightSign_;
  u8 ids[]={1,2},a[]={acc_,acc_};
  if(positionAcceleration_>0) for(int i=0;i<2;++i)
    a[i]=uint8_t(std::clamp(std::lround(positionAcceleration_*lyo_d42::stepsPerMeter/(100.0*accMultiplier_[i])),1L,254L));
  u16 targets[]={u16(target_[0]),u16(target_[1])};
  u16 speeds[]={u16(lyo_d42::speedRegister(moveSpeed_)),u16(lyo_d42::speedRegister(moveSpeed_))};
  u16 currents[]={current_,current_};
  turnaround(); driver_.SyncWritePosEx(ids,2,targets,speeds,a,currents);
  for(int id=1;id<=2;++id) { turnaround(); if(!good(driver_.EnableTorque(id,1))) return false; }
  const double v=speeds[0]*lyo_d42::speedUnit;
  const double acceleration=100.0*std::min(int(a[0])*accMultiplier_[0],int(a[1])*accMultiplier_[1]);
  segmentLimit_=uint32_t(1000*(steps/v+v/acceleration+3));
  segmentAt_=millis(); segment_=true; state_="position"; stopSent_=false;
  return true;
}
void LyoD42::tick() {
  // Acquire the physical bus before state, consistently with handle().
  BusGuard bus; std::lock_guard<std::mutex> lock(mutex_);
  const uint32_t now=millis();
  if(urgentStop_.exchange(false)) { stop("stopped"); return; }
  if(debug_) return;
  if(releasePending_) {
    if(uint32_t(now-releaseAt_)>=100) disableOutputs();
    return;
  }
  if(enabled_ && uint32_t(now-lastHeartbeat_)>=timeout_) { stop("watchdog"); return; }
  if(!rampStep(now)) return;
  if(uint32_t(now-lastPoll_)<(enabled_?100:500)) return;
  lastPoll_=now;
  if(!readFeedback()) {
    if(enabled_) stop("feedback_fault");
    return;
  }
  if(position_ && segment_) {
    if(uint32_t(now-segmentAt_)>segmentLimit_) { stop("position_timeout"); return; }
    if(uint32_t(now-segmentAt_)>=100 && moving_[0]==0 && moving_[1]==0 &&
       std::abs(pos_[0]-target_[0])<=2 && std::abs(pos_[1]-target_[1])<=2) {
      remaining_=std::max(0.0,remaining_-segmentDistance_);
      if(!nextSegment()) stop("position_io_failed");
    }
  }
}
void LyoD42::releaseForDebug() {
  BusGuard bus; std::lock_guard<std::mutex> lock(mutex_);
  if(!debug_) stop("debug");
  debug_=true;ready_=false;
}
bool LyoD42::startMotion(uint32_t received) {
  if(releasePending_ && !disableOutputs()) return false;
  if(!taskRunning_) {motorError_="task_start_failed";return false;}
  debug_=false; ready_=prepareMotors();
  if(!ready_) {stop("enable_failed");return false;}
  if(urgentStop_.load() || uint32_t(millis()-received)>=timeout_) {stop("stopped");motorError_="expired_or_cancelled";return false;}
  position_=false;stopSent_=false;speed(0,0,50);

  enabled_=true;state_="idle";return true;
}
bool LyoD42::save() {
  Preferences p; if(!p.begin("lylumo",false)) return false;
  bool ok=p.putDouble("max_speed_v2",maxSpeed_)==sizeof(double);
  ok=(p.putDouble("min_duty_v2",minDuty_)==sizeof(double))&&ok;
  ok=(p.putUChar("can_id",canId_)==1)&&ok;
  ok=(p.putUShort("timeout_v2",timeout_)==2)&&ok;
  ok=(p.putUShort("current",current_)==2)&&ok;p.end();return ok;
}

bool LyoD42::idleDisplaySnapshot(float& batteryVoltage) {
  std::unique_lock<std::mutex> lock(mutex_,std::try_to_lock);
  if(!lock.owns_lock() || enabled_ || debug_ || releasePending_ || urgentStop_.load()) return false;
  batteryVoltage=-1;
  for(int i=0;i<2;++i) if(valid_[i] && uint32_t(millis()-feedbackAt_[i])<1000 && voltage_[i]>0) {
    batteryVoltage=voltage_[i]*0.1f;break;
  }
  return true;
}
void LyoD42::snapshot(JsonDocument& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  out["T"]=808; out["ok"]=true; out["ready"]=ready_; out["enabled"]=enabled_; out["state"]=state_;
  out["release_pending"]=releasePending_;
  out["hill_hold"]=holdEnabled_; out["holding"]=hold_; out["debug"]=debug_;
  out["motor_error"]=motorError_; out["motor_id"]=errorId_;
  out["left_mps"]=left_; out["right_mps"]=right_; out["remaining_m"]=remaining_;
  out["led"]=light_; out["min_duty"]=minDuty_; out["max_speed"]=maxSpeed_;
  out["can_id"]=canId_.load(); out["timeout_ms"]=timeout_; out["current"]=current_;
  for(int i=0;i<2;++i) {
    auto w=out["wheels"][i].to<JsonObject>();
    w["id"]=i+1; w["valid"]=valid_[i] && uint32_t(millis()-feedbackAt_[i])<1000;
    if(w["valid"].as<bool>()) {
      w["current_valid"]=currentValid_[i] && uint32_t(millis()-currentReadAt_)<1500;
      if(w["current_valid"].as<bool>()) w["current_raw"]=currentRaw_[i];
      w["mps"]=lyo_d42::speedMps(velocity_[i]);
      w["temperature_c"]=temperature_[i]; w["voltage"]=voltage_[i]*0.1; w["status"]=fault_[i];
    }
  }
  if(WiFi.status()==WL_CONNECTED) out["sta_rssi"]=WiFi.RSSI();
  else out["sta_rssi"]=nullptr;
  out["ap_clients"]=WiFi.softAPgetStationNum();
}
bool LyoD42::handle(const JsonDocument& in,JsonDocument& out,uint32_t received) {
  int t=in["T"]|-1;
  if(t<800||t>811) return false;
  if(t==808) { snapshot(out); if(in["rid"].is<uint32_t>()) out["rid"]=in["rid"]; return true; }
  out["T"]=t; out["ok"]=false; if(in["rid"].is<uint32_t>()) out["rid"]=in["rid"];
  BusGuard bus; std::lock_guard<std::mutex> lock(mutex_);
  auto fail=[&](const char* e){out["error"]=e;return true;};
  if(t==800) { stop("stopped"); out["ok"]=true; return true; }
  if(t==801 && in["enable"].is<bool>() && !in["enable"].as<bool>()) {stop("disabled");out["ok"]=true;return true;}
  if(t==807) {
    if(!number(in,"brightness",0,1)) return fail("brightness must be 0..1");
    light_=in["brightness"].as<double>(); ledcWrite(21,lyo_d42::lightDuty(light_,minDuty_));
  } else if(t==809) {
    if(enabled_) return fail("disable chassis before configuration");
    if((!in["can_id"].isNull()&&!integer(in,"can_id",1,127)) ||
       (!in["max_speed"].isNull()&&!number(in,"max_speed",0.01,lyo_d42::maxSpeed)) ||
       (!in["min_duty"].isNull()&&!number(in,"min_duty",0.01,1)) ||
       (!in["timeout_ms"].isNull()&&!integer(in,"timeout_ms",100,60000)) ||
       (!in["current"].isNull()&&!integer(in,"current",1,1000)))
      return fail("can_id 1..127, max_speed .01.. 0.32, min_duty .01..1, timeout_ms 100..60000, current 1..1000");
    if(!in["can_id"].isNull()) canId_=in["can_id"].as<int>();
    if(!in["max_speed"].isNull()) maxSpeed_=in["max_speed"];
    if(!in["min_duty"].isNull()) minDuty_=in["min_duty"];
    if(!in["timeout_ms"].isNull()) timeout_=in["timeout_ms"];
    if(!in["current"].isNull()) current_=in["current"];
    if(!save()) return fail("RAM settings applied, but NVS save failed; read settings before retrying");
    ledcWrite(21,lyo_d42::lightDuty(light_,minDuty_));
  } else {
    if(uint32_t(millis()-received)>=timeout_) return fail("expired command");
    if(urgentStop_.load()) return fail("stop pending");
    if(t==801) {
      if(!in["enable"].is<bool>()) return fail("enable must be boolean");
      if(!in["enable"].as<bool>()) { stop("disabled"); out["ok"]=true; return true; }
      if(enabled_) return fail("already enabled; stop before re-enabling");
      if(!startMotion(received)) { out["motor_id"]=errorId_; return fail(motorError_); }
    } else if(t==811) {
      if(!in["hold"].is<bool>()) return fail("hold must be boolean");
      holdEnabled_=in["hold"].as<bool>();
      if(holdEnabled_) {
        // Arming while driving must not interrupt the current target.
        if(!segment_ && !ramp_ && left_==0 && right_==0) {
          if(!enabled_ && !startMotion(received)) return fail(motorError_);
          if(!enterHold()) return fail(motorError_);
        }
      } else if(hold_ || (!segment_ && !ramp_ && left_==0 && right_==0)) stop("stopped");
    } else if(t==810) {
      if(!enabled_) return fail("chassis disabled");
    } else {
      const bool physical=!in["accel_mps2"].isNull();
      if(physical && (!number(in,"accel_mps2",0.01-1e-8,5+1e-7) || !in["acc"].isNull()))
        return fail("accel_mps2 must be .01..5 m/s^2; do not combine with legacy acc");
      if(!in["soft_stop"].isNull() && !in["soft_stop"].is<bool>()) return fail("soft_stop must be boolean");
      if((in["soft_stop"]|false) && !physical) return fail("soft_stop requires accel_mps2");
      uint8_t acceleration=15;
      if(!in["acc"].isNull()) { if(!integer(in,"acc",1,254)) return fail("acc must be 1..254"); acceleration=in["acc"]; }
      if(t>=802&&t<=804) {
        double l=0,r=0;
        if(t==802||t==803) {
          if(!number(in,"left",-1000,1000)||!number(in,"right",-1000,1000)) return fail("finite left and right required");
          l=in["left"];r=in["right"]; if(t==802) {l=lyo_d42::rpmToMps(l);r=lyo_d42::rpmToMps(r);}
        } else {
          if(!number(in,"v",-10,10)||!number(in,"w",-100,100)) return fail("v (m/s) and w (rad/s) required");
          l=in["v"].as<double>()-in["w"].as<double>()*lyo_d42::track/2;
          r=in["v"].as<double>()+in["w"].as<double>()*lyo_d42::track/2;
        }
        if(position_ && !hold_) return fail("stop and re-enable before switching out of position motion");
        const bool zero=l==0 && r==0;
        const bool smooth=physical && (in["soft_stop"]|true);
        if(zero && (!smooth || (appliedLeft_==0 && appliedRight_==0))) {
          if(holdEnabled_ && !enabled_ && !startMotion(received)) return fail(motorError_);
          if(!finishStop()) return fail(motorError_);
          lastHeartbeat_=received;out["ok"]=true;return true;
        }
        if(!leaveHold()) return fail(motorError_);
        if(!enabled_ && !startMotion(received)) {out["motor_id"]=errorId_;return fail(motorError_);}
        lyo_d42::limit(l,r,maxSpeed_);left_=l;right_=r;
        if(physical) {
          if(!ramp_) rampAt_=millis();
          rampAcceleration_=std::clamp(in["accel_mps2"].as<double>(),0.01,5.0);ramp_=true;
          state_=zero?"decelerating":"velocity";
        } else {
          ramp_=false;
          if(!speed(l,r,acceleration)){stop("motion_io_failed");return fail(motorError_);}
          state_="velocity";
        }
        out["left_mps"]=lyo_d42::speedMps(lyo_d42::speedRegister(l));out["right_mps"]=lyo_d42::speedMps(lyo_d42::speedRegister(r));
      } else if(t==805||t==806) {
        if(!hold_ && (ramp_||position_||std::abs(velocity_[0])+std::abs(velocity_[1])>0||left_!=0||right_!=0)) return fail("position move requires stationary wheels");
        const char* key=t==805?"distance":"degrees";
        if(!number(in,key,t==805?-100:-36000,t==805?100:36000)||!number(in,"speed",0.005,maxSpeed_)) return fail("distance/degrees and speed .005..max_speed required");
        if(!enabled_ && !startMotion(received)) return fail(motorError_);
        hold_=false;
        double distance=in[key]; if(t==806) distance=lyo_d42::turnWheelDistance(distance);
        remaining_=std::abs(distance); rightSign_=distance>=0?1:-1;leftSign_=t==805?rightSign_:-rightSign_;
        positionAcceleration_=physical?in["accel_mps2"].as<double>():0;
        moveSpeed_=in["speed"];acc_=acceleration;
        if(!setMode(0)) { stop("mode_failed"); return fail("mode change failed"); }
        position_=true;
        if(!nextSegment()) { stop("position_io_failed"); return fail("position start failed"); }
        out["completed"]=!segment_;
      }
    }
    lastHeartbeat_=received;
  }
  out["ok"]=true; return true;
}
