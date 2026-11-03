import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),errors=[],commands=[];
const state={version:'DEMO',csrf:'PRIVATE-CSRF',fresh:false,connected:false,busy:false,error:'',targets_fresh:false,write_transport:'rs485',requires_configuration:false,
 identity_status:'unsupported_model',observed_controller:'ecoMAX OTHER DEMO',
 system:{controllerID:'REFERENCE ONLY',uid:'PRIVATE-UID'},host:'',values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{},targets:{},controls:{},econet_health:{fresh:true,api_ok:false},esp_network:{connected:true,ssid:'PRIVATE-SSID'},schedules:{ecomaxSchedules:{}},pellet:{}};
let job=null,report=null,fail=false,expired=false,invalid=false;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  let value,status=200;if(req.method==='POST')commands.push(url.pathname);
  if(url.pathname==='/api/session')value={csrf:'demo'};
  else if(url.pathname==='/api/state')value={...state,job};
  else if(url.pathname==='/api/panel/clock')value={ok:true};
  else if(url.pathname==='/api/updates')value={current:'DEMO',phase:'idle'};
  else if(url.pathname==='/api/compatibility/scan'){
   const chunks=[];for await(const c of req)chunks.push(c);const body=JSON.parse(Buffer.concat(chunks));assert.deepEqual(Object.keys(body),['active_reads']);assert.equal(typeof body.active_reads,'boolean');
   if(fail){status=409;value={message:'Trwa operacja DEMO'};}
   else{const id='demo-'+commands.length;
    report={format:'ecomax-compatibility',schema:1,read_only:true,report_id:id,partial:!body.active_reads,active_reads:body.active_reads,received_replies:body.active_reads?15:0,read_results:[],warnings:[],observed_controller:state.observed_controller};
    job={id,kind:'compatibility',status:'done',result:{report_id:id,state:'ready'}};value={job};
   }
  }else if(url.pathname==='/api/job')value={job,busy:false};
  else if(url.pathname==='/api/compatibility'){
   if(expired){status=404;value={message:'Raport nie jest dostepny DEMO'};}
   else{assert.equal(url.searchParams.get('id'),report.report_id);value=invalid?{...report,report_id:'old'}:report;}
  }
  res.writeHead(value?status:404,{'Content-Type':'application/json'}).end(JSON.stringify(value||{error:'DEMO endpoint disabled'}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg','.woff2':'font/woff2'};res.writeHead(200,{'Content-Type':types[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  const page=await browser.newPage({viewport:{width,height:900},acceptDownloads:true});page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.goto(origin);await page.locator('#compatibility-blocked').waitFor({state:'visible'});
  assert.equal(await page.locator('#model').textContent(),state.observed_controller);
  if(width===320){await page.locator('#panel-tools summary').click();await page.locator('#compatibility-open').click();}
  else await page.locator('#compatibility-blocked-open').click();
  await page.locator('#compatibility-dialog').waitFor({state:'visible'});
  assert.equal(await page.locator('#compatibility-active').isChecked(),true);
  assert.equal(await page.locator('#compatibility-start').isEnabled(),true,'no profile or targets needed to read');
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+1),false);
  assert.equal(await page.locator('#compatibility-dialog').evaluate(el=>el.scrollWidth>el.clientWidth+1),false);
  await page.screenshot({path:`test-results/compatibility-${width}.png`});
  if(width===390)await page.locator('#compatibility-active').uncheck();
  const downloading=page.waitForEvent('download');await page.locator('#compatibility-start').click();const download=await downloading;
  const data=JSON.parse(await readFile(await download.path(),'utf8'));assert.equal(data.active_reads,width!==390);assert.equal(data.observed_controller,'ecoMAX OTHER DEMO');assert.doesNotMatch(JSON.stringify(data),/PRIVATE|REFERENCE/);
  await page.waitForFunction(()=>document.querySelector('#compatibility-status').textContent.startsWith('Pobrano raport'));
  let downloaded=0;page.on('download',()=>downloaded++);
  for(const failure of ['busy','expired','invalid']){
   fail=failure==='busy';expired=failure==='expired';invalid=failure==='invalid';const before=commands.filter(x=>x==='/api/compatibility/scan').length;
   await page.locator('#compatibility-start').click();await page.waitForFunction(()=>!document.querySelector('#compatibility-close').disabled);
   const message=await page.locator('#compatibility-status').textContent();assert.match(message,failure==='busy'?/Trwa operacja/:failure==='expired'?/nie jest dostepny/:/Nieprawidłowy/);
   assert.equal(commands.filter(x=>x==='/api/compatibility/scan').length,before+1,'no automatic command resubmission');
  }
  fail=expired=invalid=false;assert.equal(downloaded,0);await page.locator('#compatibility-close').click();
  state.identity_status='identity_mismatch';await page.evaluate(()=>poll());assert.match(await page.locator('#compatibility-reason').textContent(),/inny kocioł/);
  state.identity_status='unsupported_model';await page.goto(origin+'/#settings');await page.locator('#compatibility-settings').click();assert.equal(await page.locator('#compatibility-dialog').isVisible(),true);
  await page.close();console.log('PASS compatibility unknown model, active/passive download, failure and no repeated POST: '+width+'px');
 }
 assert.deepEqual(errors,[]);assert.deepEqual([...new Set(commands)].sort(),['/api/compatibility/scan','/api/panel/clock']);
 console.log('PASS no boiler setting, transport or identity enrollment commands');
}finally{await browser.close();await new Promise(done=>server.close(done));}
