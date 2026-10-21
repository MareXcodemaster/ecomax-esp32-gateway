// Real panel on loopback with synthetic setpoints; never sends commands to a boiler.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),writes=[],points=[],errors=[];
const target={name:'CWU DEMO',before:40,effective:40,min:35,max:65,step:1,measured_key:'tempCWU',write_ready:true};
const state={version:'0.7.0-beta.25',csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
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
  }else if(url.pathname==='/api/pellet/calibrate'){
   const chunks=[];for await(const c of req)chunks.push(c);const payload=JSON.parse(Buffer.concat(chunks));points.push(payload);
   assert.equal(payload.physical_level_confirmed,true);assert.equal(payload.confirm,true);
   job={id:'point-'+points.length,kind:'pellet_calibrate',status:'done',result:{state:'acknowledged',calibration_complete:false,message:'Przyjeto punkt DEMO'}};value={job};
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
  const initialWrites=writes.length;
  const page=await browser.newPage({viewport:{width,height:900}});page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>Object.defineProperty(document,'hidden',{configurable:true,get:()=>true}));
  await page.goto(origin);await page.waitForFunction(()=>document.querySelector('.wifi-percent').textContent==='60%');
  const firstRead=await page.evaluate(()=>state.read_sequence);
  await page.waitForFunction(first=>document.hidden&&state.read_sequence>first,firstRead);
  state.target_read_ms=200;state.target_errors={};state.job={id:'old-prepare-'+width,kind:'prepare',status:'failed',finished_ms:100,error:'Brak zgodnego odczytu nastawy RS485'};
  await page.evaluate(()=>poll());assert.equal(await page.locator('#notice').isVisible(),false,'old recovered preparation failure is not resurrected');
  state.job={id:'new-preflight-'+width,kind:'save',setting_id:'1281',dispatch_started:false,status:'failed',finished_ms:300,error:'Brak zgodnego odczytu nastawy RS485'};
  await page.evaluate(()=>poll());assert.equal(await page.locator('#notice').isVisible(),true);assert.match(await page.locator('#notice').getAttribute('class'),/error/);
  state.target_read_ms=400;await page.evaluate(()=>poll());assert.doesNotMatch(await page.locator('#notice').getAttribute('class'),/error/);
  assert.match(await page.locator('#notice').innerText(),/Poprzedniej zmiany nie wyslano/);assert.equal(writes.length,initialWrites,'recovery never submits a setpoint');
  delete state.job;await page.evaluate(()=>notice(''));
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
   assert.equal(writes.length,initialWrites+1);assert.equal(writes.at(-1).value,42);await other.evaluate(()=>poll());
   assert.equal(await other.evaluate(()=>state.targets['1281'].before),42);await other.close();
   state.esp_network.connected=false;await page.evaluate(()=>poll());assert.equal(await page.locator('.wifi-percent').textContent(),'--%');
   assert.equal(await page.locator('.wifi-bars .active').count(),0);state.esp_network.connected=true;
  }
  const advanced={id:'rm:158',name:'Blokada kot\u0142a od termostatu',before:1,min:0,max:1,step:1,options:[{value:0,label:'Nie'},{value:1,label:'Tak'}],value_kind:'enum',risk:true,advanced:true,generic:true};
  state.targets['rm:158']=advanced;state.menu=[{index:158,name:advanced.name,advanced:true,in_structure:true,locked:false,write_enabled:true,write_id:advanced.id,metadata:{value:1},enum_text:'Tak'}];
  await page.evaluate(()=>poll());await page.evaluate(()=>selectView('service'));
  await page.locator('[data-section=service] .parameter-actions [data-view=advanced]').click();
  assert.equal(await page.locator('[data-section=advanced]').isVisible(),true);
  await page.getByRole('button',{name:'Zmie\u0144: '+advanced.name,exact:true}).click();
  assert.equal(await page.locator('#setting-save').isDisabled(),true);
  assert.match(await page.locator('#setting-risk').innerText(),/zaawansowanego/);
  await page.locator('#setting-option').selectOption('0');await page.locator('#setting-confirm').check();
  assert.equal(await page.locator('#setting-save').isEnabled(),true);
  const start=writes.length;await page.locator('#setting-save').click();await page.waitForFunction(()=>!document.querySelector('#setting-dialog').open);
  assert.equal(writes.length,start+1);assert.equal(writes.at(-1).confirm_advanced,true);assert.equal(writes.at(-1).confirm_installation,true);
  assert.equal(writes.at(-1).value,0);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+1),false);
  await page.screenshot({path:`test-results/advanced-menu-${width}.png`});
  state.menu=[];delete state.targets['rm:158'];
  state.pellet={fresh:true,level_percent:68,boiler_calibration_available:true};await page.evaluate(()=>poll());await page.evaluate(()=>selectView('pellet'));
  await page.locator('#pellet-full').click();assert.equal(await page.locator('#pellet-point-submit').isDisabled(),true);
  await page.locator('#pellet-point-confirm').check();await page.locator('#pellet-point-submit').click();
  await page.waitForFunction(()=>!document.querySelector('#pellet-point-dialog').open);
  assert.equal(points.at(-1).point,100);const count=points.length;
  await page.locator('#pellet-empty').click();await page.locator('#pellet-point-cancel').click();assert.equal(points.length,count);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+1),false);
  state.pellet.boiler_calibration_available=false;await page.evaluate(()=>poll());assert.equal(await page.locator('#pellet-full').isDisabled(),true);
  const beforeCal=writes.length,pointsBeforeCal=points.length;
  state.pellet.level_percent=null;state.pellet.level_calibrating=true;state.curr.fuelLevel=null;state.curr.fuelCalibration=true;
  await page.evaluate(()=>poll());assert.equal(await page.locator('#pellet-level').textContent(),'Kalibracja');
  assert.equal(await page.locator('#pellet-hopper').getAttribute('aria-label'),'Poziom pelletu: kalibracja');
  await page.evaluate(()=>selectView('live'));
  assert.equal(await page.locator('[data-member=fuelLevel] .tile-value').textContent(),'Kalibracja');
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+1),false);
  await page.screenshot({path:`test-results/fuel-calibration-${width}.png`});
  state.curr.fuelCalibration=false;state.curr.fuelLevel=0;await page.evaluate(()=>poll());
  assert.equal(await page.locator('[data-member=fuelLevel] .tile-value').textContent(),'0 %');
  assert.equal(writes.length,beforeCal);assert.equal(points.length,pointsBeforeCal,'rendering CAL never sends a command');
  state.pellet.level_calibrating=false;state.pellet.level_percent=0;
  await page.close();console.log('PASS real panel: background polling, Wi-Fi bars, responsive layout, click target, '+width+'px');
 }
 assert.deepEqual(errors,[]);console.log('PASS two-client external change: draft preserved, one atomic Save, readback synchronized');
}finally{await browser.close();await new Promise(done=>server.close(done));}
