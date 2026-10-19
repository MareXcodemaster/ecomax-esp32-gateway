import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';

const root=resolve('src/panel_web'),now=Math.floor(Date.now()/1000),errors=[],requests=[];
const state={version:'DEMO',csrf:'PRIVATE-CSRF',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO',uid:'PRIVATE-UID'},host:'',values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{mode:3,tempCO:70,tempCWU:45,tempCWUSet:45,lambdaLevel:73},targets:{},controls:{},econet_health:{fresh:true,api_ok:false},
 esp_network:{connected:true,rssi_dbm:-70,ssid:'PRIVATE-SSID',ip:'192.0.2.10',hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{},last_read_utc:new Date().toISOString()};
const columns=['time','native','mode','co','oxygen','gapBefore'];let fail=false,delay=false;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  requests.push([req.method,url.pathname,url.search]);let value,status=200;
  if(url.pathname==='/api/session')value={csrf:'demo'};
  else if(url.pathname==='/api/state'||url.pathname==='/api/export')value=state;
  else if(url.pathname==='/api/panel/clock')value={ok:true};
  else if(url.pathname==='/api/updates')value={current:'DEMO',phase:'idle'};
  else if(url.pathname==='/api/history'){
   if(delay)await new Promise(done=>setTimeout(done,500));
   if(fail){status=503;value={message:'DEMO blad archiwum'};}
   else{const from=Number(url.searchParams.get('from'))||now-3600,to=Number(url.searchParams.get('to'))||now,rows=[];
    for(let t=from;t<=to;t+=60)rows.push([t,1,3,70,7.3,0]);
    const hour=Math.floor(from/3600)*3600;
    value={export_raw:url.searchParams.get('raw')==='1',columns,rows,archive:{ready:true,interval_seconds:60,samples:4321},settings_history:{ready:true,dropped:0,bad_files:0,pending:0,events:[{schema:1,kind:'baseline',time:hour,event_id:hour+':0',values:{'menu:12':32},uid:'PRIVATE-UID'}]}};
   }
  }
  res.writeHead(value?status:404,{'Content-Type':'application/json'}).end(JSON.stringify(value||{error:'DEMO endpoint disabled'}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg','.woff2':'font/woff2'};res.writeHead(200,{'Content-Type':types[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  const page=await browser.newPage({viewport:{width,height:900},acceptDownloads:true});page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.goto(origin);await page.locator('#panel-tools summary').click();await page.locator('#analysis-export').click();
  await page.locator('#analysis-dialog').waitFor({state:'visible'});assert.equal(await page.locator('#analysis-hours').inputValue(),'72');
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  assert.equal(await page.locator('#analysis-dialog').evaluate(el=>el.scrollWidth>el.clientWidth+1),false);
  await page.screenshot({path:`test-results/analysis-export-${width}.png`});
  const hours=width===1280?72:1;await page.locator('#analysis-hours').selectOption(String(hours));
  const downloadPromise=page.waitForEvent('download');await page.locator('#analysis-download').click();const download=await downloadPromise;
  const report=JSON.parse(await readFile(await download.path(),'utf8'));
  assert.equal(report.format,'ecomax-combustion-analysis');assert.equal(report.history.sample_count,hours*60+1);assert.equal(report.history.additional_downsampling,false);assert.doesNotMatch(JSON.stringify(report),/PRIVATE/);
  assert.equal(report.settings_history.event_count,hours);assert.equal(report.settings_history.events[0].values['menu:12'],32);assert.equal(report.settings_history.quality.ready,true);
  await page.waitForFunction(()=>document.querySelector('#analysis-status').textContent.startsWith('Zapisano JSON:'));
  assert.equal(await page.locator('#analysis-download').isEnabled(),true);await page.locator('#analysis-close').click();
  if(width<=900)await page.locator('#nav-toggle').click();
  await page.locator('[data-view="history"]').click();await page.locator('#history-json').click();
  fail=true;await page.locator('#analysis-download').click();await page.waitForFunction(()=>document.querySelector('#analysis-status').textContent.startsWith('Nie pobrano pliku:'));
  assert.equal(await page.locator('#analysis-download').isEnabled(),true);fail=false;delay=true;
  await page.locator('#analysis-download').click();await page.locator('#analysis-cancel').click();
  await page.waitForFunction(()=>!document.querySelector('#analysis-dialog').open&&!document.querySelector('#analysis-download').disabled);delay=false;
  await page.close();console.log('PASS JSON download, all recorded samples, privacy, failure and cancellation: '+width+'px');
 }
 assert.deepEqual(errors,[]);
 assert.deepEqual([...new Set(requests.filter(([method])=>method!=='GET').map(([,path])=>path))],['/api/panel/clock']);
 console.log('PASS export is read-only: no boiler, settings or configuration commands');
}finally{await browser.close();await new Promise(done=>server.close(done));}
