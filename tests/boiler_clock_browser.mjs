import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),errors=[],writes=[];
const state={version:'0.7.0-beta.56',csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:false,write_transport:'rs485',requires_configuration:false,
system:{controllerID:'ecoMAX860P-LZ TOUCH'},host:'',values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],curr:{mode:4},targets:{},controls:{start:false,stop:false},econet_health:{fresh:false,api_ok:false},
esp_network:{connected:true,rssi_dbm:-60,hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{},last_read_utc:new Date().toISOString()};
let clock,offline=false;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  let data=null;
  if(req.method==='POST'){
   let body='';for await(const chunk of req)body+=chunk;
   const value=JSON.parse(body||'{}');writes.push({path:url.pathname,value});
   if(url.pathname==='/api/boiler-clock'){
    if('automatic'in value){clock.automatic=value.automatic;clock.ntp_synchronized=value.automatic;}
    else{clock.pending=true;clock.message='Oczekiwanie na synchronizację';setTimeout(()=>{clock.pending=false;clock.message='Zsynchronizowano; potwierdzone przez panel kotła';},500);}
    data=clock;
   }else data={ok:true};
  }else if(url.pathname==='/api/session')data={csrf:'demo'};
  else if(url.pathname==='/api/state')data=offline?null:{...state,boiler_clock:clock};
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
  offline=false;clock={fresh:true,supported:true,available:true,automatic:false,pending:false,ntp_synchronized:false,device_time:'2026-11-07 21:25:00',age_ms:500,message:'Zegar kotła jest zgodny'};
  const page=await browser.newPage({viewport:{width,height:960}});page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.goto(origin+'/#settings');await page.waitForFunction(()=>document.querySelector('#boiler-clock-time').textContent.startsWith('21:25'));
  assert.equal(await page.locator('#header-clock').count(),0);
  assert.equal(await page.locator('#boiler-clock-auto').isChecked(),false);
  await page.locator('#boiler-clock-auto').check();await page.waitForFunction(()=>document.querySelector('#boiler-clock-source').textContent.includes('jest aktualny'));
  await page.locator('#boiler-clock-sync').click();await page.waitForFunction(()=>document.querySelector('#boiler-clock-status').textContent.includes('potwierdzone'));
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.screenshot({path:'test-results/boiler-clock-'+width+'.png'});
  clock.fresh=false;await page.evaluate(()=>poll());await page.waitForFunction(()=>document.querySelector('#boiler-clock-time').textContent==='—');
  offline=true;await page.evaluate(()=>poll());assert.equal(await page.locator('#boiler-clock-sync').isEnabled(),false);
  await page.close();console.log('PASS boiler clock '+width+'px: observed time, automatic toggle, verified sync, stale/offline, no overflow');
 }
 assert.deepEqual(errors,[]);assert.ok(writes.every(x=>['/api/panel/clock','/api/boiler-clock'].includes(x.path)));
}finally{await browser.close();await new Promise(done=>server.close(done));}
