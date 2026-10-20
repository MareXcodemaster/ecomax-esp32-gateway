import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';

const root=resolve('src/panel_web'),errors=[],requests=[];
const version=JSON.parse(await readFile('package.json','utf8')).version;
const fixture=JSON.parse(await readFile('tests/fixtures/device_info.json','utf8'));
let offline=false;
const state={version,csrf:'demo',fresh:false,connected:false,busy:false,error:'',targets_fresh:false,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO'},host:'',values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{},targets:{},controls:{start:false,stop:false},econet_health:{fresh:true,api_ok:false},
 esp_network:{connected:true,rssi_dbm:-60,hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{},last_read_utc:new Date().toISOString(),esp_device:structuredClone(fixture)};
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  requests.push([req.method,url.pathname]);
  const data=url.pathname==='/api/session'?{csrf:'demo'}:url.pathname==='/api/state'?(offline?{message:'DEMO offline'}:state):url.pathname==='/api/panel/clock'?{ok:true}:url.pathname==='/api/updates'?{current:version,phase:'idle'}:null;
  res.writeHead(url.pathname==='/api/state'&&offline?503:data?200:404,{'Content-Type':'application/json'}).end(JSON.stringify(data||{}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg','.woff2':'font/woff2'};res.writeHead(200,{'Content-Type':types[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});
await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  state.esp_device=structuredClone(fixture);offline=false;
  const page=await browser.newPage({viewport:{width,height:960}});
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.goto(origin);
  await page.waitForFunction(()=>document.querySelector('#read-time').textContent.startsWith('Odczyt'));
  if(!await page.locator('[data-view="settings"]').isVisible())await page.locator('#nav-toggle').click();
  await page.locator('[data-view="settings"]').click();
  await page.waitForFunction(()=>document.querySelector('#esp-resources-status').textContent==='Aktualne dane ESP');
  assert.equal(await page.locator('.esp-memory-item meter:visible').count(),4);
  assert.match(await page.locator('#esp-memory-firmware-used').innerText(),/53,3%/);
  assert.match(await page.locator('#esp-memory-ram-used').innerText(),/78,1%/);
  assert.equal(await page.locator('#esp-memory-psram').getAttribute('max'),'8388608');
  assert.match(await page.locator('#esp-device-values').innerText(),/16 MiB/);
  assert.match(await page.locator('#esp-device-values').innerText(),/1 d 02:03:04/);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  for(const box of await page.locator('.esp-memory-item').evaluateAll(items=>items.map(el=>({width:el.clientWidth,scroll:el.scrollWidth}))))assert.ok(box.scroll<=box.width+1);
  await page.locator('.esp-resources').evaluate(el=>el.scrollIntoView({block:'start'}));
  assert.ok((await page.locator('.esp-resources h3').boundingBox()).y>(await page.locator('header').boundingBox()).height);
  await page.screenshot({path:'test-results/device-info-'+width+'.png'});
  offline=true;
  await page.waitForFunction(()=>document.querySelector('#esp-resources-status').textContent==='Brak aktualnego odczytu ESP');
  assert.equal(await page.locator('.esp-memory-item meter:visible').count(),0);
  assert.equal(await page.locator('#esp-device-values').innerText(),'');
  offline=false;state.esp_device.internal_ram.used_bytes+=1024;state.esp_device.internal_ram.free_bytes-=1024;state.esp_device.uptime_ms+=1000;state.esp_device.reset_reason=9;
  await page.waitForFunction(()=>document.querySelector('#esp-memory-ram').value===205824);
  assert.match(await page.locator('#esp-device-values').innerText(),/Spadek napiecia zasilania/);
  state.esp_device.history={ready:false,loading:false,error:'DEMO brak archiwum'};state.esp_device.psram={total_bytes:0,used_bytes:0,free_bytes:0};
  state.esp_device.chip='<img src=x onerror=alert(1)>';
  await page.waitForFunction(()=>document.querySelector('#esp-history-storage-status').textContent.includes('DEMO brak archiwum'));
  assert.equal(await page.locator('#esp-memory-history').isVisible(),false);assert.equal(await page.locator('#esp-memory-psram').isVisible(),false);
  assert.equal(await page.locator('#esp-device-values img').count(),0);
  await page.close();console.log('PASS ESP resources '+width+'px: memory pools, offline boiler, lost ESP, recovery, missing storage and text-only fields');
 }
 assert.deepEqual(errors,[]);
 assert.deepEqual([...new Set(requests.filter(([method])=>method!=='GET').map(([,path])=>path))],['/api/panel/clock']);
 assert.equal(requests.some(([,path])=>!['/api/session','/api/state','/api/panel/clock','/api/updates'].includes(path)),false);
 console.log('PASS device information uses existing state polling; no extra device or boiler requests');
}finally{await browser.close();await new Promise(done=>server.close(done));}
