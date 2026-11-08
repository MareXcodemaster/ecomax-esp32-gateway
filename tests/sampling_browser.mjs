import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),errors=[],writes=[];
const state={version:'0.7.0-beta.56',csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:false,write_transport:'rs485',requires_configuration:false,
system:{controllerID:'ecoMAX860P-LZ TOUCH'},host:'',values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],curr:{mode:4},targets:{},controls:{start:false,stop:false},econet_health:{fresh:false,api_ok:false},
esp_network:{connected:true,rssi_dbm:-60,hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{},last_read_utc:new Date().toISOString()};
let clock,offline=false,stateReads=0,uploads=0,uploadBytes=0,rejectOta=false,rebooted=false;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  let data=null;
  if(url.pathname==='/api/ota'){let size=0;for await(const chunk of req)size+=chunk.length;uploads++;uploadBytes=size;assert.equal(req.headers['x-lazar-request'],'1');assert.equal(req.headers['x-tester-token'],'demo');if(!rejectOta)rebooted=true;res.writeHead(rejectOta?400:200,{'Content-Type':'application/json'}).end(JSON.stringify({message:rejectOta?'Obraz odrzucony':'Restart'}));return;}
  if(req.method==='POST'){
   let body='';for await(const chunk of req)body+=chunk;
   const value=JSON.parse(body||'{}');writes.push({path:url.pathname,value});
   if(url.pathname==='/api/sampling'){state.sampling=value;data=value;}else if(url.pathname==='/api/boiler-clock'){
    if('automatic'in value){clock.automatic=value.automatic;clock.ntp_synchronized=value.automatic;}
    else{clock.pending=true;clock.message='Oczekiwanie na synchronizację';setTimeout(()=>{clock.pending=false;clock.message='Zsynchronizowano; potwierdzone przez panel kotła';},500);}
    data=clock;
   }else data={ok:true};
  }else if(url.pathname==='/api/session')data={csrf:'demo'};
  else if(url.pathname==='/api/state'){stateReads++;data=offline?null:{...state,boiler_clock:clock};}
  else if(url.pathname==='/api/status')data={firmware:rebooted?'0.7.0-beta.57':'0.7.0-beta.56',uptime_ms:rebooted?500:100000};
  else if(url.pathname==='/api/updates')data={current:state.version,phase:'idle'};
  else if(url.pathname==='/api/tailnet')data={enabled:false,phase:'disabled'};
  res.writeHead(data?200:503,{'Content-Type':'application/json'}).end(JSON.stringify(data||{message:'offline'}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg','.woff2':'font/woff2'};res.writeHead(200,{'Content-Type':types[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL||undefined});await mkdir('test-results',{recursive:true});
try{
 for(const width of [390,1280]){
  rebooted=false;offline=false;clock={fresh:false,supported:false};state.sampling={boiler_ms:3000,panel_ms:2000,history_ms:60000};
  const page=await browser.newPage({viewport:{width,height:960}});page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.goto(origin+'/#settings');await page.waitForFunction(()=>!document.querySelector('#sampling-panel_ms').disabled);
  assert.equal(await page.locator('#sampling-cancel').isVisible(),false);
  assert.equal(await page.locator('#sampling-panel_ms').inputValue(),'2');
  await page.locator('#sampling-panel_ms-unit').selectOption('ms');assert.equal(await page.locator('#sampling-panel_ms').inputValue(),'2000');
  assert.equal(await page.locator('#sampling-cancel').isVisible(),false);
  await page.locator('#sampling-panel_ms').fill('800');await page.evaluate(()=>poll());
  assert.equal(await page.locator('#sampling-panel_ms').inputValue(),'800');
  await page.locator('#sampling-history_ms').fill('45');await page.locator('#sampling-save').click();
  await page.waitForFunction(()=>document.querySelector('#sampling-status').textContent.includes('Zapisano'));
  assert.equal(state.sampling.panel_ms,800);assert.equal(state.sampling.history_ms,45000);
  assert.equal(await page.evaluate(()=>espPollInterval),800);
  const reads=stateReads;await page.waitForTimeout(1800);assert.ok(stateReads>=reads+2);
  await page.reload();await page.waitForFunction(()=>!document.querySelector('#sampling-panel_ms').disabled);
  assert.equal(await page.locator('#sampling-panel_ms').inputValue(),'0.8');
  await page.locator('#sampling-defaults').click();await page.locator('#sampling-save').click();
  await page.waitForFunction(()=>espPollInterval===2000);assert.equal(state.sampling.history_ms,60000);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.locator('#sampling-title').scrollIntoViewIfNeeded();await page.screenshot({path:'test-results/sampling-'+width+'.png'});
  // Malformed/merged images never reach the upload endpoint.
  const payload=Buffer.alloc(2048);payload[0]=0xe9;payload.writeUInt16LE(9,12);payload.writeUInt32LE(0xabcd5432,32);payload.write('0.7.0-beta.57',48);payload.write('lazar_stage0',80);
  await page.locator('#manual-ota-file').setInputFiles({name:'firmware-merged.bin',mimeType:'application/octet-stream',buffer:payload});
  await page.waitForFunction(()=>document.querySelector('#manual-ota-status').textContent.includes('merged'));
  assert.equal(await page.locator('#manual-ota-install').isEnabled(),false);
  await page.locator('#manual-ota-file').setInputFiles({name:'firmware-ota.bin',mimeType:'application/octet-stream',buffer:payload});
  await page.waitForFunction(()=>document.querySelector('#manual-ota-status').textContent.includes('Wybrano'));
  assert.equal(await page.locator('#manual-ota-install').isEnabled(),false);
  await page.locator('#manual-ota-confirm').check();
  const beforeUploads=uploads;
  rejectOta=true;await page.locator('#manual-ota-install').click();
  await page.waitForFunction(()=>document.querySelector('#manual-ota-status').textContent.includes('Nie zainstalowano'));
  assert.equal(uploads,beforeUploads+1);assert.equal(await page.evaluate(()=>actionBusy),false);
  rejectOta=false;await page.locator('#manual-ota-install').click();
  await page.waitForFunction(()=>document.querySelector('#manual-ota-status').textContent.includes('potwierdzona'));
  assert.equal(uploads,beforeUploads+2);assert.equal(uploadBytes,2048);
  await page.waitForTimeout(1500);await page.waitForFunction(()=>!document.querySelector('#sampling-panel_ms').disabled);
  assert.equal(await page.locator('#sampling-cancel').isVisible(),false);
  await page.close();console.log('PASS '+width+'px: units, dirty form, persistent config, real polling interval, defaults, OTA validation/error/upload/reboot');
 }
 assert.deepEqual(errors,[]);
}finally{await browser.close();await new Promise(done=>server.close(done));}
