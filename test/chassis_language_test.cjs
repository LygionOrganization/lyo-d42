const {chromium}=require('playwright');const assert=require('node:assert/strict');
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'C:/Program Files/Google/Chrome/Application/chrome.exe'});
try {
 const page=await browser.newPage(),errors=[],commands=[];
 page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>{if(r.url().endsWith('/api/cmd'))commands.push(r.postData())});
 await page.route('**/api/chassis',route=>route.fulfill({json:{state:'disabled',max_speed:0.32,timeout_ms:3000,current:200,can_id:1,min_duty:.4,sta_rssi:-50,ap_clients:1,tilt:{valid:true,roll:0,pitch:0},wheels:[{id:1,valid:true,mps:0,voltage:20,temperature_c:36,current_valid:true,current_raw:123},{id:2,valid:true,mps:0,voltage:20,temperature_c:37,current_valid:true,current_raw:125}]}}));
 await page.goto('http://127.0.0.1:8765');await page.waitForFunction(()=>document.querySelector('#driveTemperature').textContent.includes('36'));
 assert(!/[\u4e00-\u9fff]/.test(await page.locator('#chassis').innerText()));
 assert.equal(await page.locator('#driveState').innerText(),'Disabled');assert.match(await page.locator('#driveCurrent').innerText(),/123/);assert.equal(await page.locator('#driveSoftStop').count(),1);
 await page.locator('#driveAcc').fill('0.42');await page.locator('#driveMove input[name=value]').fill('1.5');
 const before=commands.length;
 await page.locator('#languageToggle').click();
 assert.equal(await page.locator('html').getAttribute('lang'),'zh-CN');
 assert.equal(await page.locator('#chassis h2').first().innerText(),'驾驶');
 assert.equal(await page.locator('#driveState').innerText(),'已停用');assert.match(await page.locator('#driveAttitude').innerText(),/^横滚 \/ 俯仰:/);
 assert.equal(await page.locator('#drivePad').getAttribute('aria-label'),'驾驶摇杆');
 assert.equal(await page.locator('#driveAcc').inputValue(),'0.42');assert.equal(await page.locator('#driveMove input[name=value]').inputValue(),'1.5');assert.equal(commands.length,before);
 await page.screenshot({path:'.pio/chassis-zh-desktop.png',fullPage:true});
 await page.reload();assert.equal(await page.locator('html').getAttribute('lang'),'zh-CN');
 await page.setViewportSize({width:390,height:844});
 for(const lang of ['zh','en']){
  if(lang==='en')await page.locator('#languageToggle').click();
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  await page.screenshot({path:`.pio/chassis-${lang}-mobile.png`,fullPage:true});
 }
 assert(!/[\u4e00-\u9fff]/.test(await page.locator('#chassis').innerText()));
 await page.locator('[data-page=settings]').click();assert(!/[\u4e00-\u9fff]/.test(await page.locator('#heartbeatForm').innerText()));
 assert.deepEqual(errors,[]);console.log('PASS chassis language toggle: static and live text, form preservation, no command side effects, saved preference, joystick accessibility, mobile layout');
}finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
