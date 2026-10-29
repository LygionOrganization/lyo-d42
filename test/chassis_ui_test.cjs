// Run without a browser: exercise actual UI handlers against a mocked transport.
const vm=require('node:vm'),fs=require('node:fs'),assert=require('node:assert/strict');
const html=fs.readFileSync('web/index.html','utf8'),nodes={},listeners={},posts=[],sent=[],intervals=[];
for(const m of html.matchAll(/id="([^"]+)"/g))nodes[m[1]]={style:{},value:'',hidden:false,checked:false,elements:new Proxy({},{get:(o,k)=>o[k]??=( {value:'',max:''} )})};
nodes.driveSpeed.value='0.05';nodes.driveAcc.value='0.42';
const telemetry={state:'disabled',wheels:[{id:1,valid:true,mps:0,voltage:20,temperature_c:36},{id:2,valid:true,mps:0,voltage:20,temperature_c:37}],tilt:{valid:true,roll:0,pitch:0},max_speed:0.32,timeout_ms:3000,min_duty:.4,can_id:1,current:200,ap_clients:1,sta_rssi:-50};
const context={t:value=>({"横滚 / 俯仰":"Roll / Pitch","左 / 右轮速":"L / R","已停用":"Disabled"}[value]||value),console,AbortSignal,Blob,document:{hidden:false,hasFocus:()=>true,getElementById:id=>{assert(nodes[id],id);return nodes[id]},addEventListener:(k,f)=>listeners[k]=f},window:{addEventListener:(k,f)=>listeners[k]=f},navigator:{sendBeacon:()=>{}},setInterval:f=>intervals.push(f),setTimeout:()=>1,clearTimeout:()=>{},received:()=>{},send:async c=>sent.push(c),fetch:async(url,o)=>{if(o?.body)posts.push(JSON.parse(o.body));return {ok:true,json:async()=>telemetry}}};
vm.runInNewContext(fs.readFileSync('web/lyo_d42.js','utf8'),context);
const flush=()=>new Promise(r=>setImmediate(r));
(async()=>{await flush();assert(!nodes.driveStop);assert(nodes.driveTemperature.textContent.includes('ID1: 36'));assert(nodes.driveTemperature.textContent.includes('ID2: 37'));assert.equal(nodes.driveAttitude.textContent,'Roll / Pitch: 0.0° / 0.0°');assert.equal(nodes.driveSpeed.max,0.32);
const event={code:'KeyW',target:{tagName:'BODY'},preventDefault(){}};listeners.keydown(event);await flush();assert.equal(posts.at(-1).T,804);assert.equal(posts.at(-1).accel_mps2,.42);assert.equal(posts.at(-1).soft_stop,false);assert.equal(posts.at(-1).v,.05);
listeners.keyup(event);await flush();assert.equal(posts.at(-1).v,0);
await nodes.driveHold.onchange({target:{checked:true}});await intervals[0]();assert.equal(posts.at(-1).T,804);assert.equal(posts.at(-1).v,0);listeners.keydown(event);await flush();assert.equal(posts.at(-1).v,.05);listeners.keyup(event);await flush();assert.equal(posts.at(-1).v,0);listeners.blur();await flush();assert.equal(posts.at(-1).T,800);
nodes.driveSoftStop.checked=true;listeners.keydown(event);await flush();listeners.keyup(event);await flush();assert.equal(posts.at(-1).soft_stop,true);const count=posts.length;await intervals[0]();assert(posts.length>count);assert.equal(posts.at(-1).v,0);
await nodes.heartbeatForm.onsubmit({preventDefault(){},target:{elements:{timeout_ms:{value:'5000'}}}});assert.equal(sent.at(-1).T,809);assert.equal(sent.at(-1).timeout_ms,5000);
assert(html.includes('id="imuYaw"'));console.log('PASS chassis UI: temperatures, tilt, speed limit, immediate driving, acceleration, hill hold heartbeat, blur stop, timeout settings, debug yaw');})().catch(e=>{console.error(e);process.exitCode=1});
