// Real panel on loopback with synthetic setpoints; never sends commands to a boiler.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),writes=[],errors=[];
const target={name:'CWU DEMO',before:40,effective:40,min:35,max:65,step:1,measured_key:'tempCWU',write_ready:true};
const state={version:'0.7.0-beta.18',csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO'},values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{mode:0,tempCWU:35,tempCWUSet:40,ecoSterMode1:2},targets:{1281:target},controls:{},econet_health:{fresh:true,api_ok:false},
 esp_network:{connected:true,rssi_dbm:-70,ssid:'DEMO',ip:'192.0.2.10',hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{}};
let job=null,readSequence=0;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  let value;
  if(url.pathname==='/api/session')value={csrf:'demo'};
  else if(url.pathname==='/api/panel/clock')value={ok:true};
  else if(url.pathname==='/api/state')value={...state,read_sequence:++readSequence};
  else if(url.pathname==='/api/updates')value={phase:'up_to_date',current:state.version,automatic:false,checked:Date.now()/1000};
  else if(url.pathname==='/api/write/save'){
   const chunks=[];for await(const c of req)chunks.push(c);const payload=JSON.parse(Buffer.concat(chunks));writes.push(payload);
   assert.equal(payload.before,undefined);assert.equal(payload.confirm,true);
   const actual=state.targets[payload.id];actual.before=actual.effective=payload.value;state.curr.tempCWUSet=payload.value;
   job={id:String(writes.length),kind:'save',status:'done',result:{state:'verified',message:'Zapis potwierdzony'}};value={job};
  }else if(url.pathname==='/api/job')value={job,busy:false};
  res.writeHead(value?200:404,{'Content-Type':'application/json'}).end(JSON.stringify(value||{error:'DEMO: disabled'}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg'};res.writeHead(200,{'Content-Type':types[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});
await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  const page=await browser.newPage({viewport:{width,height:900}});page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>Object.defineProperty(document,'hidden',{configurable:true,get:()=>true}));
  await page.goto(origin);await page.waitForFunction(()=>document.querySelector('.wifi-percent').textContent==='60%');
  const firstRead=await page.evaluate(()=>state.read_sequence);
  await page.waitForFunction(first=>document.hidden&&state.read_sequence>first,firstRead);
  assert.equal(await page.locator('.wifi-bars span').count(),5);assert.equal(await page.locator('.wifi-bars .active').count(),3);
  const signal=page.locator('#wifi-signal');assert.match(await signal.getAttribute('title'),/-70 dBm/);
  assert.equal(await signal.evaluate(el=>el.scrollWidth>el.clientWidth+1),false);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+1),false);
  await page.screenshot({path:`test-results/live-save-signal-${width}.png`});
  await signal.click();assert.equal(await page.locator('#wifi-dialog').isVisible(),true);await page.keyboard.press('Escape');
  if(width===1280){
   const other=await browser.newPage();await other.goto(origin);await other.waitForFunction(()=>!!window.EcoMirror);
   await page.evaluate(()=>EcoMirror.openTemperature('1281'));await page.locator('#temperature-value').fill('42');
   target.before=target.effective=45;await page.evaluate(()=>poll());
   assert.equal(await page.locator('#temperature-value').inputValue(),'42');assert.equal(await page.locator('#temperature-update').textContent(),'');
   await page.locator('#temperature-form [type=submit]').click();await page.waitForFunction(()=>!document.querySelector('#temperature-dialog').open);
   assert.equal(writes.length,1);assert.equal(writes[0].value,42);await other.evaluate(()=>poll());
   assert.equal(await other.evaluate(()=>state.targets['1281'].before),42);await other.close();
   state.esp_network.connected=false;await page.evaluate(()=>poll());assert.equal(await page.locator('.wifi-percent').textContent(),'--%');
   assert.equal(await page.locator('.wifi-bars .active').count(),0);state.esp_network.connected=true;
  }
  await page.close();console.log('PASS real panel: background polling, Wi-Fi bars, responsive layout, click target, '+width+'px');
 }
 assert.deepEqual(errors,[]);console.log('PASS two-client external change: draft preserved, one atomic Save, readback synchronized');
}finally{await browser.close();await new Promise(done=>server.close(done));}
