// Lyo-D42 UI. Control commands never wait behind the generic JSON console result.
(()=>{
 const el=id=>document.getElementById(id),keys=new Set();
 let controlError='',owned=false,position=false,stopping=false,x=0,y=0,pointer=null,busy=false,generation=0,configured=false;
 const visible=()=>!document.hidden&&!el('chassis').hidden&&document.hasFocus();
 const error=e=>{controlError=e.message;renderLabels();};
 let latest=null,offline=false;
 const states={decelerating:'减速停车中',starting:'启动中',disabled:'已停用',idle:'待机',velocity:'行驶中',position:'定距运动中',completed:'已完成',stopped:'已停车',hill_hold:'驻坡保持中',watchdog:'心跳超时',feedback_fault:'电机反馈故障',position_timeout:'定距运动超时',initialization_failed:'初始化失败',enable_failed:'使能失败',mode_failed:'模式切换失败',motion_io_failed:'运动通信失败',position_io_failed:'定距启动失败',hold_failed:'驻坡失败',task_start_failed:'控制任务启动失败',debug:'调试模式'};
 function renderLabels(){
   if(offline){el('driveState').textContent=t('离线');el('driveAttitude').textContent=t('姿态：数据过期');return;}
   if(!latest)return;const s=latest;
   el('driveState').textContent=s.motor_error?'ID'+s.motor_id+': '+s.motor_error:(controlError?t(controlError):t(states[s.state]||s.state));
   el('driveWheels').textContent=t('左 / 右轮速')+': '+(s.wheels||[]).map(w=>w.valid?Number(w.mps).toFixed(3)+' m/s':'—').join(' / ');
   el('driveCurrent').textContent=t('电流原始值')+': '+(s.wheels||[]).map(w=>'ID'+w.id+' '+(w.valid&&w.current_valid&&Number.isFinite(w.current_raw)?w.current_raw:'—')).join(' / ');
   el('driveAttitude').textContent=s.tilt?.valid?t('横滚 / 俯仰')+': '+[s.tilt.roll,s.tilt.pitch].map(v=>Number(v).toFixed(1)+'°').join(' / '):t('姿态：无有效反馈');
 }
 window.addEventListener('languagechange',renderLabels);
 async function post(command){
   const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(command),signal:AbortSignal.timeout(700)});
   if(!r.ok)throw Error('HTTP '+r.status);
 }
 function resetAxes(){keys.clear();x=y=0;pointer=null;el('driveKnob').style.transform='';}
 function stop(){hillHold=false;el('driveHold').checked=false;++generation;owned=false;position=false;stopping=false;resetAxes();post({T:800}).catch(error);}
 let hillHold=false;
 el('driveHold').onchange=async e=>{const requested=e.target.checked;try{await send({T:811,hold:requested,reliable:true});hillHold=requested;owned=owned||hillHold;}catch(err){e.target.checked=hillHold;error(err);}};
 el('driveSpeed').oninput=()=>{el('driveSpeedText').textContent=el('driveSpeed').value+' m/s';};
 el('driveAcc').oninput=()=>{el('driveAccText').textContent=el('driveAcc').value+' m/s²';};
 el('heartbeatForm').onsubmit=async e=>{e.preventDefault();try{await send({T:809,timeout_ms:Number(e.target.elements.timeout_ms.value),reliable:true},el('heartbeatResult'));configured=false;}catch(err){el('heartbeatResult').textContent=err.message;}};
 let lightTimer;
 el('driveLight').oninput=()=>{const brightness=Number(el('driveLight').value)/100;el('driveLightText').textContent=Math.round(brightness*100)+'%';clearTimeout(lightTimer);lightTimer=setTimeout(()=>post({T:807,brightness,reliable:true}).catch(error),100);};
 const pad=el('drivePad');
 function point(e){const r=pad.getBoundingClientRect();x=Math.max(-1,Math.min(1,(e.clientX-r.left-r.width/2)/(r.width/2)));y=Math.max(-1,Math.min(1,-(e.clientY-r.top-r.height/2)/(r.height/2)));const n=Math.hypot(x,y);if(n>1){x/=n;y/=n;}el('driveKnob').style.transform=`translate(${x*65}px,${-y*65}px)`;}
 pad.onpointerdown=e=>{if(position||pointer!==null)return;owned=true;controlError="";e.preventDefault();pointer=e.pointerId;pad.setPointerCapture(pointer);point(e);driveUpdate();};
 pad.onpointermove=e=>{if(e.pointerId===pointer)point(e);};
 pad.onpointerup=pad.onpointercancel=pad.onlostpointercapture=()=>{resetAxes();if(owned&&!position)driveUpdate();};
 function editing(e){return /INPUT|TEXTAREA|SELECT/.test(e.target.tagName)||e.target.isContentEditable;}
 document.addEventListener('keydown',e=>{if(!visible()||editing(e))return;if(e.code==='Space'){e.preventDefault();stop();return;}if(['KeyW','KeyA','KeyS','KeyD','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.code)){e.preventDefault();if(!position){owned=true;controlError="";keys.add(e.code);driveUpdate();}}});
 document.addEventListener('keyup',e=>{keys.delete(e.code);driveUpdate();});
 window.addEventListener('blur',()=>{if(owned)stop();});
 document.addEventListener('visibilitychange',()=>{if(document.hidden&&owned)stop();});
 window.addEventListener('hashchange',()=>{if(el('chassis').hidden&&owned)stop();});
 window.addEventListener('pagehide',()=>{if(owned)navigator.sendBeacon('/api/cmd',new Blob(['{"T":800}'],{type:'application/json'}));owned=false;});
 async function driveUpdate(){
   if(!owned||busy)return;if(!visible()){stop();return;}
   const forward=pointer===null?Number(keys.has('KeyW')||keys.has('ArrowUp'))-Number(keys.has('KeyS')||keys.has('ArrowDown')):y;
   const turn=pointer===null?Number(keys.has('KeyA')||keys.has('ArrowLeft'))-Number(keys.has('KeyD')||keys.has('ArrowRight')):-x;
   const speed=Number(el('driveSpeed').value);busy=true;
   const zero=forward===0&&turn===0;
   stopping=!position&&zero&&el('driveSoftStop').checked;
   if(!position&&!hillHold&&zero&&!stopping)owned=false;
   try{await post(position?{T:810}:{T:804,v:forward*speed,w:turn*speed*2/0.159,accel_mps2:Number(el('driveAcc').value),soft_stop:el('driveSoftStop').checked});}catch(e){error(e);stop();}finally{busy=false;}
 }
 setInterval(driveUpdate,100);
 el('driveMove').onsubmit=async e=>{
   e.preventDefault();resetAxes();owned=true;position=true;
   const f=new FormData(e.target),T=Number(f.get('mode')),value=Number(f.get('value'));
   try{await send({T,[T===805?'distance':'degrees']:value,speed:Number(f.get('speed')),accel_mps2:Number(f.get('acc')),reliable:true});}catch(e){position=false;error(e);}
 };
 el('canSend').onsubmit=async e=>{e.preventDefault();const f=new FormData(e.target),id=parseInt(f.get('id'),16),ext=f.get('ext')==='1',text=f.get('bytes').trim(),tokens=text?text.split(/\s+/):[];
   try{if(tokens.length>8||tokens.some(t=>!/^([0-9a-f]{2})$/i.test(t))||id>(ext?0x1fffffff:0x7ff))throw Error('Invalid CAN ID/data');await send({T:501,id,ext,data:tokens.map(t=>parseInt(t,16)),reliable:true},el('canResult'));}catch(e){el('canResult').textContent=e.message;}};
 el('chassisConfig').onsubmit=async e=>{e.preventDefault();const cmd={T:809,reliable:true};for(const[k,v]of new FormData(e.target))cmd[k]=Number(v);try{await send(cmd,el('canResult'));configured=false;}catch(e){el('canResult').textContent=e.message;}};
 const originalReceived=received,canLines=[];
 received=function(value){if(value.T===501&&value.direction==='rx'){canLines.push(`${value.ext?'EXT':'STD'} ${Number(value.id).toString(16)} ${(value.data||[]).map(v=>v.toString(16).padStart(2,'0')).join(' ')}`);if(canLines.length>100)canLines.shift();el('canLog').textContent=canLines.join('\n');}originalReceived(value);};
 async function poll(){
   try{if(!document.hidden){const r=await fetch('/api/chassis',{cache:'no-store',signal:AbortSignal.timeout(1000)});if(!r.ok)throw Error('Telemetry HTTP '+r.status);const s=await r.json();
     latest=s;offline=false;renderLabels();el('driveStatus').textContent=JSON.stringify(s,null,2);
     const w=s.wheels?.[0];el('driveVoltage').textContent=w?.valid?w.voltage.toFixed(1)+' V':'—';el('driveRssi').textContent=s.sta_rssi===null?'—':s.sta_rssi+' dBm';el('driveClients').textContent=s.ap_clients;
     hillHold=!!s.hill_hold;el('driveHold').checked=hillHold;if(stopping&&!s.enabled){owned=false;stopping=false;}if(position&&s.state==='completed'){owned=hillHold;position=false;resetAxes();}
     el('driveSpeed').max=Math.floor(s.max_speed*2000)/2000;el('driveSpeed').oninput();el('driveMove').elements.speed.max=s.max_speed;
     if(!configured){for(const key of ['can_id','timeout_ms','max_speed','min_duty','current'])el('chassisConfig').elements[key].value=s[key];el('heartbeatForm').elements.timeout_ms.value=s.timeout_ms;configured=true;}
     el('driveTemperature').textContent=(s.wheels||[]).map(w=>'ID'+w.id+': '+(w.valid&&Number.isFinite(w.temperature_c)?w.temperature_c+' °C':'—')).join(' / ');


   }}catch(e){offline=true;renderLabels();el('driveVoltage').textContent='—';if(owned)stop();}finally{setTimeout(poll,500);}
 }
 poll();
})();
