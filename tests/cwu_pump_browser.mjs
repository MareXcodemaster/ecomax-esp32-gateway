import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),posts=[],external=[],errors=[];
const applied=new Set();
let state={enabled:false,configured:false,paired:false,armed:false,host:'',mac:'',alias:'',model:'',fresh:false,on:null,temperature:46,sensor_fresh:true,lower:40,upper:45,mode:0,schedule_enabled:false,week:Array.from({length:7},()=>Array(6).fill(0)),reason:'Sterowanie nieaktywne',status:'Nie skonfigurowano',devices:[],scan_busy:false};
let failPost=false,failGet=false,settingsResponseUpper=null;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname==='/api/cwu-pump'){
  if(req.method==='GET'&&failGet){res.writeHead(503).end('{}');return;}
  if(req.method==='POST'){
   let raw='';for await(const data of req)raw+=data;const body=JSON.parse(raw);posts.push(body);
   if(applied.has(body.request_id)){res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify(state));return;}applied.add(body.request_id);
   if(body.action==='scan')state.devices=[{host:'192.168.1.222',mac:'021122334455',model:'P110',protocol:'KLAP'}];
   if(body.action==='feature')state={...state,enabled:body.enabled};
   if(body.action==='connect')state={...state,host:body.host,mac:body.mac,configured:true,paired:true,model:'P110',protocol:'TPAP',alias:'Pompa DEMO',fresh:true,on:false,status:'Polaczono lokalnie',energy:{supported:true,fresh:true,history_fresh:true,power_w:4,today_kwh:.01,month_kwh:.12,week_kwh:.07,days:Array.from({length:7},(_,i)=>({date:'2026-01-0'+(i+1),kwh:.01}))}};
   if(body.action==='settings'){state={...state,...body,armed:true};if(settingsResponseUpper!==null){state.upper=settingsResponseUpper;settingsResponseUpper=null;}if(failPost){failPost=false;res.destroy();return;}}
  }
  res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify(state));return;
 }
 if(url.pathname==='/'){
  res.end('<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/style.css"><link rel="stylesheet" href="/mirror.css"><link rel="stylesheet" href="/week.css"><link rel="stylesheet" href="/typography.css"><link rel="stylesheet" href="/cwu_pump.css"><main></main><script src="/lucide.js"></script><script>function icons(){lucide.createIcons();} const EcoMirror={clock:s=>String(Math.floor(s/2)).padStart(2,"0")+":"+(s%2?"30":"00")};async function request(path,payload){const r=await fetch(path,{method:payload?"POST":"GET",headers:{"Content-Type":"application/json"},body:payload?JSON.stringify(payload):undefined});if(!r.ok)throw Error("HTTP "+r.status);return r.json();}</script><script src="/cwu_pump.js"></script><script>EcoCwuPump.init();document.querySelector("section").hidden=false;EcoCwuPump.show(true);</script>');return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname));if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{res.writeHead(200,{'Content-Type':{'.css':'text/css','.js':'text/javascript','.woff2':'font/woff2','.jpg':'image/jpeg','.png':'image/png'}[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({headless:true});
try{
 const page=await browser.newPage();page.on('pageerror',error=>errors.push(error.message));
 await page.route('**/*',route=>{if(!route.request().url().startsWith(origin+'/')){external.push(route.request().url());return route.abort();}return route.continue();});
 for(const width of [320,390,1280]){
  await page.setViewportSize({width,height:960});await page.goto(origin);await page.locator('#pump-link').filter({hasText:'Nie skonfigurowano'}).waitFor({state:'attached'});
  assert.equal(await page.locator('#pump-save').isDisabled(),true);assert.equal(await page.locator('#pump-schedule-panel').isHidden(),true);
  assert.equal(await page.locator('#pump-content').isHidden(),true);await page.locator('#pump-enabled').check();await page.locator('#pump-content').waitFor();
  await page.locator('#pump-connect').locator('summary').click();await page.locator('#pump-scan').click();
  await page.locator('#pump-device-select option[value="021122334455"]').waitFor({state:'attached'});
  await page.locator('#pump-device-select').selectOption('021122334455');assert.equal(await page.locator('#pump-host').inputValue(),'192.168.1.222');
  await page.locator('#pump-password').fill('demo-pump-password');await page.locator('#pump-connect-save').click();
  await page.locator('#pump-message').filter({hasText:'Dane przyj'}).waitFor();assert.equal(await page.locator('#pump-password').inputValue(),'');
  assert.equal(posts.at(-1).username,'');assert.match(await page.locator('#pump-device').innerText(),/^TPAP/);
  assert.equal(await page.locator('#pump-power').innerText(),'4,0 W');assert.equal(await page.locator('#pump-energy-week').innerText(),'0,070 kWh');assert.equal(await page.locator('#pump-energy-days li').count(),7);
  assert.deepEqual(await page.locator('.pump-controls button').allTextContents(),['Automatycznie','Ręcznie','Wyłączona']);
  assert.equal(await page.locator('.pump-controls button').evaluateAll(els=>els.every(el=>el.scrollWidth<=el.clientWidth)),true,'mode labels fit inside their controls');
  await page.locator('#pump-manual').click();await page.locator('#pump-manual-controls').waitFor();assert.equal(posts.at(-1).mode,3,'entering manual while OFF does not start the pump');
  await page.locator('#pump-start').click();await page.locator('#pump-message').filter({hasText:'Tryb zapisany'}).waitFor();
  assert.doesNotMatch(await page.locator('#pump-message').innerText(),/Oczekiwanie/);
  assert.equal(posts.at(-1).mode,2);assert.equal(await page.locator('#pump-tap').getAttribute('data-state'),'off','a command is not a confirmed relay state');
  state.on=true;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.locator('#pump-tap[data-state="on"]').waitFor();
  assert.equal(await page.locator('.tap-water').evaluate(el=>getComputedStyle(el).display),'block');
  assert.equal(await page.locator('.tap-motion').evaluate(el=>getComputedStyle(el).animationName),'pump-current');
  await page.emulateMedia({reducedMotion:'reduce'});assert.equal(await page.locator('.tap-motion').evaluate(el=>getComputedStyle(el).animationName),'none');await page.emulateMedia({reducedMotion:'no-preference'});
  assert.equal(await page.locator('[data-section="pump"] video').count(),0);
  await page.locator('#pump-auto').click();await page.locator('#pump-auto-panel').waitFor();assert.equal(await page.locator('#pump-schedule-enabled').isVisible(),true,'automatic schedule toggle is immediately visible');
  assert.equal(await page.locator('#pump-reset').isHidden(),true,'opening automatic settings has no unsaved changes');
  assert.equal(await page.locator('#pump-upper').evaluate(el=>el.getBoundingClientRect().width<=160),true,'temperature input stays compact');
  assert.equal(await page.locator('#pump-settings').evaluate(el=>el.querySelector('.actions').getBoundingClientRect().top-el.querySelector('.pump-thresholds').getBoundingClientRect().bottom>=16),true,'actions have a clear gap below temperature inputs');
  settingsResponseUpper=46;await page.locator('#pump-auto').click();await page.waitForFunction(()=>document.getElementById('pump-upper').value==='46');
  assert.equal(await page.locator('#pump-reset').isHidden(),true,'a saved value returned during a mode response is not a user edit');
  state.upper=45;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.waitForFunction(()=>document.getElementById('pump-upper').value==='45');
  state.upper=46;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.waitForFunction(()=>document.getElementById('pump-upper').value==='46');
  assert.equal(await page.locator('#pump-reset').isHidden(),true,'a refreshed saved value is not a user edit');
  state.upper=45;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.waitForFunction(()=>document.getElementById('pump-upper').value==='45');
  await page.locator('#pump-upper').fill('46');assert.equal(await page.locator('#pump-reset').isVisible(),true);
  await page.locator('#pump-upper').fill('45');assert.equal(await page.locator('#pump-reset').isHidden(),true,'typing back the saved temperature clears changes');
  await page.locator('#pump-schedule-enabled').check();assert.equal(await page.locator('#pump-reset').isVisible(),true);
  await page.locator('#pump-schedule-enabled').uncheck();assert.equal(await page.locator('#pump-reset').isHidden(),true,'restoring the saved switch clears changes');
  assert.equal(await page.locator('#pump-schedule-enabled').evaluate(el=>el.getBoundingClientRect().width>=48),true,'schedule switch has a visible touch target');
  await page.locator('#pump-upper').fill('47');await page.locator('#pump-manual').click();await page.locator('#pump-manual-controls').waitFor();
  assert.equal(posts.at(-1).mode,2,'entering manual while ON preserves ON');
  await page.locator('#pump-stop').click();await page.waitForTimeout(150);assert.equal(posts.at(-1).mode,3);assert.equal(posts.at(-1).upper,45,'quick action uses saved rules');
  assert.equal(await page.locator('#pump-upper').inputValue(),'47','quick action preserves unsaved rules');
  state.on=false;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.locator('#pump-tap[data-state="off"]').waitFor();
  assert.equal(await page.locator('.tap-water').evaluate(el=>getComputedStyle(el).display),'none');
  assert.equal(await page.locator('#pump-manual').getAttribute('aria-pressed'),'true','manual OFF remains in manual mode');
  await page.locator('#pump-off').click();await page.waitForTimeout(150);assert.equal(posts.at(-1).mode,0);assert.equal(await page.locator('#pump-manual-controls').isHidden(),true);
  await page.locator('#pump-auto').click();await page.locator('#pump-auto-panel').waitFor();await page.locator('#pump-reset').click();await page.waitForTimeout(150);assert.equal(posts.at(-1).mode,1);
  assert.equal(await page.locator('#pump-reset').isHidden(),true,'cancel restores saved rules and hides itself');
  await page.locator('#pump-schedule-enabled').check();
  assert.equal(await page.locator('#pump-week button').count(),336);
  await page.locator('#pump-week button[data-row="0"][data-slot="0"]').click();
  assert.equal(await page.locator('#pump-week button').first().getAttribute('aria-checked'),'true');
  await page.locator('#pump-save').click();await page.locator('#pump-message').filter({hasText:'Ustawienia zapisane'}).waitFor();
  const saved=posts.at(-1);assert.equal(saved.action,'settings');assert.equal(saved.schedule_enabled,true);assert.equal(saved.week[0][0],128);assert.equal(saved.mode,1);
  assert.equal(await page.locator('#pump-reset').isHidden(),true,'successful save clears changes');
  await page.locator('#pump-week button').first().click();assert.equal(await page.locator('#pump-reset').isVisible(),true);
  await page.locator('#pump-week button').first().click();assert.equal(await page.locator('#pump-reset').isHidden(),true,'restoring a schedule cell clears changes');
  await page.locator('#pump-schedule-enabled').uncheck();await page.locator('#pump-save').click();await page.locator('#pump-message').filter({hasText:'Ustawienia zapisane'}).waitFor();
  assert.equal(posts.at(-1).schedule_enabled,false);assert.equal(posts.at(-1).week[0][0],128);
  await page.locator('#pump-upper').fill('47');await page.evaluate(()=>EcoCwuPump.refresh(true));await page.waitForTimeout(150);
  assert.equal(await page.locator('#pump-upper').inputValue(),'47');await page.locator('#pump-reset').click();assert.equal(await page.locator('#pump-upper').inputValue(),'45');
  await page.locator('#pump-schedule-enabled').check();
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  state.energy.fresh=false;state.energy.history_fresh=false;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.locator('#pump-power').getByText('Brak odczytu').waitFor();assert.equal(await page.locator('#pump-energy-week').innerText(),'Brak odczytu');
  state.energy.supported=false;state.model='P100';await page.evaluate(()=>EcoCwuPump.refresh(true));await page.locator('#pump-energy').waitFor({state:'hidden'});
  failGet=true;await page.evaluate(()=>EcoCwuPump.refresh(true));await page.locator('#pump-tap[data-state="unknown"]').waitFor();assert.equal(await page.locator('.tap-water').evaluate(el=>getComputedStyle(el).display),'none');failGet=false;
  await page.locator('#pump-enabled').uncheck();await page.locator('#pump-content').waitFor({state:'hidden'});assert.equal(posts.at(-1).action,'feature');assert.equal(state.on,false,'disabling feature does not send a relay command');
  state={...state,enabled:false,configured:false,paired:false,armed:false,host:'',mac:'',fresh:false,on:null,status:'Nie skonfigurowano',mode:0,schedule_enabled:false,week:Array.from({length:7},()=>Array(6).fill(0))};
 }
 await page.reload();await page.locator('#pump-enabled').check();await page.locator('#pump-content').waitFor();await page.locator('#pump-connect summary').click();await page.locator('#pump-host').fill('192.168.1.222');await page.locator('#pump-username').fill('demo@example.invalid');await page.locator('#pump-password').fill('demo-pump-password');await page.locator('#pump-connect-save').click();await page.locator('#pump-message').filter({hasText:'Dane przyj'}).waitFor();
 failPost=true;const count=applied.size;await page.locator('#pump-manual').click();await page.waitForTimeout(500);assert.equal(applied.size,count+1,'a transport retry cannot apply the operation twice');
 assert.equal(await page.evaluate(()=>localStorage.length+sessionStorage.length),0);assert.deepEqual(external,[]);assert.deepEqual(errors,[]);
 console.log('PASS pump panel: opt-in, one-click ON/OFF/Auto, animation follows confirmed state, lost read stops water, preserved unsaved rules, 320/390/1280 px, discovery/MAC, 336 cells, unknown response not repeated. No real devices accessed.');
}finally{await browser.close();await new Promise(done=>server.close(done));}
