import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {execFile} from 'node:child_process';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),encoder=resolve('test-results/panel-sync-encoder'+(process.platform==='win32'?'.exe':''));
const version=JSON.parse(await readFile('package.json','utf8')).version;
const state={version,csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO'},values:{},names:{},menu:[{index:0,name:'Test',metadata:{value:40,minv:35,maxv:65,edit:true},read_available:true,write_enabled:true}],
 enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{mode:0,tempCWU:35,tempCWUSet:40,ecoSterMode1:2},targets:{1281:{name:'CWU DEMO',before:40,effective:40,min:35,max:65,step:1,measured_key:'tempCWU',write_ready:true}},
 controls:{},econet_health:{fresh:true,api_ok:false},esp_network:{connected:true,rssi_dbm:-65,hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{}};
let boot='test-boot-1',offline=false,full=0,compact=0,reads=0;
const errors=[],mutations=[];
function encode(known){
 return new Promise((done,fail)=>{
  const child=execFile(encoder,[known,boot],{maxBuffer:1024*1024},(error,stdout)=>error?fail(error):done(JSON.parse(stdout)));
  child.stdin.end(JSON.stringify({...state,read_sequence:++reads}));
 });
}
const server=createServer(async(req,res)=>{
 try{
  const url=new URL(req.url,'http://localhost');
  if(url.pathname.startsWith('/api/')){
   if(req.method!=='GET')mutations.push(url.pathname);
   let value;
   if(url.pathname==='/api/session')value={csrf:state.csrf};
   else if(url.pathname==='/api/panel/clock')value={ok:true};
   else if(url.pathname==='/api/updates')value={current:version,phase:'idle'};
   else if(url.pathname==='/api/state'){
    if(offline){res.writeHead(503,{'Content-Type':'application/json'}).end('{"message":"offline DEMO"}');return;}
    assert.equal(url.searchParams.get('compact'),'1');
    value=await encode(url.searchParams.get('catalog')||'');
    value.sync.included?full++:compact++;
   }
   res.writeHead(value?200:404,{'Content-Type':'application/json'}).end(JSON.stringify(value||{}));return;
  }
  const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
  if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
  const mime={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg','.woff2':'font/woff2'};
  res.writeHead(200,{'Content-Type':mime[extname(path)]||'application/octet-stream'}).end(await readFile(path));
 }catch(e){errors.push(e.message);res.writeHead(500).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});
try{
 const pages=await Promise.all([browser.newPage({viewport:{width:390,height:900}}),browser.newPage()]);
 for(const page of pages){
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.addInitScript(()=>Object.defineProperty(document,'hidden',{get:()=>true}));
  await page.goto(origin+'/#alarms');
  await page.waitForFunction(()=>state?.targets?.['1281']?.before===40);
  await page.reload();
  await page.waitForFunction(()=>state?.targets?.['1281']?.before===40);
  assert.equal(await page.evaluate(()=>location.hash),'#alarms');
  const previous=await page.evaluate(()=>state.read_sequence);
  await page.waitForFunction(before=>document.hidden&&state.read_sequence>before,previous);
 }
 await pages[0].evaluate(()=>EcoMirror.openTemperature('1281'));
 await pages[0].locator('#temperature-value').fill('42');
 const loaded=full;
 state.targets[1281].before=45;state.curr.tempCWUSet=45;state.menu[0].metadata.value=45;
 for(const page of pages){
  await page.waitForFunction(()=>state?.targets?.['1281']?.before===45);
  assert.equal(await page.evaluate(()=>state.menu[0].metadata.value),45);
  assert.equal(await page.evaluate(()=>state.targets[1281].name),'CWU DEMO');
 }
 assert.equal(full,loaded,'changing readings must not resend the catalogue');
 assert.equal(await pages[0].locator('#temperature-value').inputValue(),'42','background polls preserve unsaved input');
 await pages[0].keyboard.press('Escape');
 state.targets[1281].max=60;state.menu[0].metadata.edit=false;state.menu[0].write_enabled=false;
 for(const page of pages){await page.waitForFunction(()=>state?.targets?.['1281']?.max===60);assert.equal(await page.evaluate(()=>state.menu[0].metadata.edit),false);}
 assert.ok(full>=loaded+2);
 boot='test-boot-2';state.csrf='new-boot-demo';state.system.controllerID='OTHER DEMO';delete state.targets[1281];state.menu=[];
 for(const page of pages){
  await page.waitForFunction(()=>state?.system?.controllerID==='OTHER DEMO');
  assert.deepEqual(await page.evaluate(()=>state.targets),{});
  assert.deepEqual(await page.evaluate(()=>state.menu),[]);
  assert.equal(await page.evaluate(()=>csrf),'new-boot-demo');
 }
 offline=true;
 for(const page of pages)await page.waitForFunction(()=>!serverReachable);
 offline=false;state.fresh=false;state.curr={};
 for(const page of pages){await page.waitForFunction(()=>serverReachable&&state.fresh===false);assert.deepEqual(await page.evaluate(()=>state.curr),{});}
 assert.ok(compact>=6);assert.deepEqual(errors,[]);
 assert.deepEqual([...new Set(mutations)],['/api/panel/clock']);
 console.log('PASS real C++ encoder + two browsers: compact background polls, external changes, draft preservation, rights/range changes, reboot, removed targets, offline/recovery and retained navigation');
 console.log(JSON.stringify({full,compact,reads}));
}finally{await browser.close();await new Promise(done=>server.close(done));}
