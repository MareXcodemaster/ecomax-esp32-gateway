import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),posts=[],external=[],errors=[];
const applied=new Set();
let state={configured:false,paired:false,armed:false,host:'',mac:'',alias:'',model:'',fresh:false,on:null,temperature:46,sensor_fresh:true,lower:40,upper:45,mode:0,schedule_enabled:false,week:Array.from({length:7},()=>Array(6).fill(0)),reason:'Sterowanie nieaktywne',status:'Nie skonfigurowano',devices:[],scan_busy:false};
let failPost=false;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname==='/api/cwu-pump'){
  if(req.method==='POST'){
   let raw='';for await(const data of req)raw+=data;const body=JSON.parse(raw);posts.push(body);
   if(applied.has(body.request_id)){res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify(state));return;}applied.add(body.request_id);
   if(body.action==='scan')state.devices=[{host:'192.168.1.222',mac:'021122334455',model:'P110',protocol:'KLAP'}];
   if(body.action==='connect')state={...state,host:body.host,mac:body.mac,configured:true,paired:true,model:'P110',protocol:'TPAP',alias:'Pompa DEMO',fresh:true,on:false,status:'Polaczono lokalnie'};
   if(body.action==='settings'){state={...state,...body,armed:true};if(failPost){failPost=false;res.destroy();return;}}
  }
  res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify(state));return;
 }
 if(url.pathname==='/'){
  res.end('<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="/style.css"><link rel="stylesheet" href="/mirror.css"><link rel="stylesheet" href="/week.css"><link rel="stylesheet" href="/typography.css"><link rel="stylesheet" href="/cwu_pump.css"><main></main><script src="/lucide.js"></script><script>function icons(){lucide.createIcons();} const EcoMirror={clock:s=>String(Math.floor(s/2)).padStart(2,"0")+":"+(s%2?"30":"00")};async function request(path,payload){const r=await fetch(path,{method:payload?"POST":"GET",headers:{"Content-Type":"application/json"},body:payload?JSON.stringify(payload):undefined});if(!r.ok)throw Error("HTTP "+r.status);return r.json();}</script><script src="/cwu_pump.js"></script><script>EcoCwuPump.init();document.querySelector("section").hidden=false;EcoCwuPump.show(true);</script>');return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname));if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{res.writeHead(200,{'Content-Type':{'.css':'text/css','.js':'text/javascript','.woff2':'font/woff2'}[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({headless:true});
try{
 const page=await browser.newPage();page.on('pageerror',error=>errors.push(error.message));
 await page.route('**/*',route=>{if(!route.request().url().startsWith(origin+'/')){external.push(route.request().url());return route.abort();}return route.continue();});
 for(const width of [320,390,1280]){
  await page.setViewportSize({width,height:960});await page.goto(origin);await page.locator('#pump-link').filter({hasText:'Nie skonfigurowano'}).waitFor();
  assert.equal(await page.locator('#pump-save').isDisabled(),true);assert.equal(await page.locator('#pump-schedule-panel').isHidden(),true);
  await page.locator('#pump-connect').locator('summary').click();await page.locator('#pump-scan').click();
  await page.locator('#pump-device-select option[value="021122334455"]').waitFor({state:'attached'});
  await page.locator('#pump-device-select').selectOption('021122334455');assert.equal(await page.locator('#pump-host').inputValue(),'192.168.1.222');
  await page.locator('#pump-password').fill('demo-pump-password');await page.locator('#pump-connect-save').click();
  await page.locator('#pump-message').filter({hasText:'Dane przyj'}).waitFor();assert.equal(await page.locator('#pump-password').inputValue(),'');
  assert.equal(posts.at(-1).username,'');assert.match(await page.locator('#pump-device').innerText(),/^TPAP/);
  await page.locator('#pump-mode').selectOption('1');await page.locator('#pump-schedule-enabled').check();
  assert.equal(await page.locator('#pump-week button').count(),336);
  await page.locator('#pump-week button[data-row="0"][data-slot="0"]').click();
  assert.equal(await page.locator('#pump-week button').first().getAttribute('aria-checked'),'true');
  await page.locator('#pump-save').click();await page.locator('#pump-message').filter({hasText:'Ustawienia zapisane'}).waitFor();
  const saved=posts.at(-1);assert.equal(saved.action,'settings');assert.equal(saved.schedule_enabled,true);assert.equal(saved.week[0][0],128);assert.equal(saved.mode,1);
  await page.locator('#pump-schedule-enabled').uncheck();await page.locator('#pump-save').click();await page.locator('#pump-message').filter({hasText:'Ustawienia zapisane'}).waitFor();
  assert.equal(posts.at(-1).schedule_enabled,false);assert.equal(posts.at(-1).week[0][0],128);
  await page.locator('#pump-upper').fill('47');await page.evaluate(()=>EcoCwuPump.refresh(true));await page.waitForTimeout(150);
  assert.equal(await page.locator('#pump-upper').inputValue(),'47');await page.locator('#pump-reset').click();assert.equal(await page.locator('#pump-upper').inputValue(),'45');
  await page.locator('#pump-mode').selectOption('1');await page.locator('#pump-schedule-enabled').check();
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  state={...state,configured:false,paired:false,armed:false,host:'',mac:'',fresh:false,on:null,status:'Nie skonfigurowano',mode:0,schedule_enabled:false,week:Array.from({length:7},()=>Array(6).fill(0))};
 }
 await page.reload();await page.locator('#pump-connect summary').click();await page.locator('#pump-host').fill('192.168.1.222');await page.locator('#pump-username').fill('demo@example.invalid');await page.locator('#pump-password').fill('demo-pump-password');await page.locator('#pump-connect-save').click();await page.locator('#pump-message').filter({hasText:'Dane przyj'}).waitFor();
 await page.locator('#pump-mode').selectOption('1');failPost=true;const count=applied.size;await page.locator('#pump-save').click();await page.waitForTimeout(500);assert.equal(applied.size,count+1,'a transport retry cannot apply the operation twice');
 assert.equal(await page.evaluate(()=>localStorage.length+sessionStorage.length),0);assert.deepEqual(external,[]);assert.deepEqual(errors,[]);
 console.log('PASS pump panel: 320/390/1280 px, discovery/MAC, local credentials cleared, 336 cells, optional schedule, preserved drafts, unknown response deduplicated. No real devices accessed.');
}finally{await browser.close();await new Promise(done=>server.close(done));}
