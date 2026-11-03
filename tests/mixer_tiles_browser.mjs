// Synthetic loopback controller only: verify that MIX2 cannot save MIX1's setting.
import assert from 'node:assert/strict';import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';import {resolve,sep,extname} from 'node:path';import {chromium} from 'playwright';
const root=resolve('src/panel_web'),writes=[],errors=[];
const version=JSON.parse(await readFile('package.json','utf8')).version;
const state={version,csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO'},values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{mode:3,mixerTemp1:44,mixerSetTemp1:45,mixerTemp2:31,mixerSetTemp2:35,ecoSterMode1:2},targets:{},controls:{},econet_health:{fresh:true,api_ok:false},schedules:{ecomaxSchedules:{}},pellet:{}};
let job=null;
const server=createServer(async(req,res)=>{const url=new URL(req.url,'http://localhost');if(url.pathname.startsWith('/api/')){
 let result=url.pathname==='/api/session'?{csrf:'demo'}:url.pathname==='/api/state'?state:url.pathname==='/api/updates'?{current:version,phase:'up_to_date'}:url.pathname==='/api/panel/clock'?{ok:true}:url.pathname==='/api/job'?{job,busy:false}:null;
 if(url.pathname==='/api/write/save'){let input='';for await(const part of req)input+=part;const payload=JSON.parse(input);writes.push(payload);assert.equal(payload.id,'rm:91');assert.equal(payload.value,36);state.targets[payload.id].before=36;state.curr.mixerSetTemp2=36;job={id:String(writes.length),kind:'save',status:'done',result:{state:'verified',message:'Zapis potwierdzony DEMO'}};result={job};}
 res.writeHead(result?200:404,{'Content-Type':'application/json'}).end(JSON.stringify(result||{}));return;
 }const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}try{res.writeHead(200,{'Content-Type':{'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg'}[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}});
await new Promise(done=>server.listen(0,'127.0.0.1',done));const browser=await chromium.launch({headless:true});
try{for(const width of [320,390,1280]){
 state.menu=[];state.targets={};state.curr.mixerSetTemp2=35;state.fresh=true;state.targets_fresh=true;
 const page=await browser.newPage({viewport:{width,height:1080}});page.on('pageerror',e=>errors.push(e.message));await page.goto('http://127.0.0.1:'+server.address().port);
 const tile=page.locator('[data-member=mixerTemp2]');await tile.waitFor();await page.evaluate(()=>{clearInterval(espPollTimer);espPollTimer=null;});await page.waitForFunction(()=>!pollBusy);
 assert.equal(await tile.locator('[data-reading=set] .tile-value').textContent(),'35,0');assert.equal(await tile.locator('[data-reading=actual] .tile-value').textContent(),'31,0');
 assert.match(await tile.locator('.tile-action').getAttribute('aria-label'),/Szczeg/);
 state.menu=[{name:'Temperatura zadana mieszacza',category:'Ustawienia mieszacza 1',unit_name:'°C',write_enabled:true,write_id:'rm:62'},{name:'Temperatura zadana mieszacza',category:'Ustawienia mieszacza 2',unit_name:'°C',write_enabled:true,write_id:'rm:91'}];
 const target=(before,name)=>({generic:true,value_kind:'number',unit:'°C',before,min:20,max:65,step:1,name,risk:true,options:[]});state.targets={'rm:62':target(45,'Temperatura zadana mieszacza 1'),'rm:91':target(35,'Temperatura zadana mieszacza 2')};
 await page.evaluate(()=>poll());assert.match(await tile.locator('.tile-action').getAttribute('aria-label'),/Zmie/);
 const before=writes.length;await tile.locator('.tile-action').click();await page.locator('#setting-dialog').waitFor({state:'visible'});assert.equal(await page.locator('#setting-title').textContent(),'Temperatura zadana mieszacza 2');
 await page.locator('#setting-number').fill('36');await page.locator('#setting-confirm').check();await page.locator('#setting-save').click();await page.waitForFunction(()=>!document.querySelector('#setting-dialog').open);
 assert.equal(writes.length,before+1);assert.equal(state.targets['rm:62'].before,45);await page.evaluate(()=>poll());assert.equal(await tile.locator('[data-reading=set] .tile-value').textContent(),'36,0');
 state.curr.mixerSetTemp2=37;state.targets['rm:91'].before=37;await page.evaluate(()=>poll());assert.equal(await tile.locator('[data-reading=set] .tile-value').textContent(),'37,0');
 state.fresh=false;await page.evaluate(()=>poll());assert.equal(await tile.locator('.tile-action').isDisabled(),true);assert.equal(await tile.locator('[data-reading=actual] .tile-value').textContent(),'---');
 assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);await page.close();console.log('PASS MIX2 tile: late catalogue, correct ID, single save, MIX1 untouched, external refresh, stale lock '+width+'px');
 }assert.deepEqual(errors,[]);}finally{await browser.close();await new Promise(done=>server.close(done));}
