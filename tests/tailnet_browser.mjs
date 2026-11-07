import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';

const root=resolve('src/panel_web'),errors=[],writes=[];
const version=JSON.parse(await readFile('package.json','utf8')).version;
const state={version,csrf:'demo',fresh:false,connected:false,busy:false,error:'',targets_fresh:false,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO'},host:'',values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{},targets:{},controls:{start:false,stop:false},econet_health:{fresh:true,api_ok:false},
 esp_network:{connected:true,rssi_dbm:-60,hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{},last_read_utc:new Date().toISOString()};
let status,fail=false;
const reset=()=>{status={enabled:false,phase:'disabled',name:'piec',key_saved:false,connected:false,policy_ready:false,ip:'',internal_free_bytes:131072};};
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  if(req.method==='POST'){
   let body='';for await(const chunk of req)body+=chunk;
   writes.push(url.pathname);
   if(url.pathname==='/api/tailnet'){
    assert.equal(req.headers['x-tester-token'],'demo');assert.equal(req.headers['x-lazar-request'],'1');
    if(fail){res.writeHead(400,{'Content-Type':'application/json'}).end(JSON.stringify({message:'DEMO: zapis nie powiódł się'}));return;}
    const value=JSON.parse(body);status={...status,enabled:value.enabled,name:value.name,key_saved:status.key_saved||!!value.auth_key,phase:value.enabled?'connecting':'disabled'};
   }
  }
  const data=url.pathname==='/api/tailnet'?status:url.pathname==='/api/session'?{csrf:'demo'}:url.pathname==='/api/state'?state:url.pathname==='/api/panel/clock'?{ok:true}:url.pathname==='/api/updates'?{current:version,phase:'idle'}:null;
  res.writeHead(data?200:404,{'Content-Type':'application/json'}).end(JSON.stringify(data||{}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.jpg':'image/jpeg','.woff2':'font/woff2'};res.writeHead(200,{'Content-Type':types[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});
await mkdir('test-results',{recursive:true});
try{
 for(const width of [390,1280]){
  reset();fail=false;
  const page=await browser.newPage({viewport:{width,height:980}});
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>route.request().url().startsWith(origin+'/')?route.continue():route.abort());
  await page.goto(origin+'/#settings');
  await page.waitForFunction(()=>document.querySelector('#tailnet-status').textContent==='Wyłączony');
  assert.equal(await page.locator('#tailnet-enabled').isChecked(),false);
  assert.equal(await page.locator('#tailnet-cancel').isVisible(),false);
  await page.locator('#tailnet-name').fill('kotlownia');
  await page.evaluate(()=>EcoTailnet.load());await page.waitForTimeout(300);
  assert.equal(await page.locator('#tailnet-name').inputValue(),'kotlownia');
  await page.locator('#tailnet-cancel').click();assert.equal(await page.locator('#tailnet-name').inputValue(),'piec');
  await page.locator('#tailnet-enabled').check();
  await page.locator('#tailnet-key').fill('synthetic-test-value');
  await page.locator('#tailnet-save').click();
  await page.waitForFunction(()=>document.querySelector('#tailnet-message').textContent==='Ustawienia zapisane.');
  assert.equal(await page.locator('#tailnet-key').inputValue(),'');
  assert.equal(await page.locator('#tailnet-cancel').isVisible(),false);
  status={...status,phase:'connected',connected:true,policy_ready:true,ip:'100.64.0.10'};
  await page.evaluate(()=>EcoTailnet.load());await page.locator('#tailnet-link').waitFor({state:'visible'});
  assert.equal(await page.locator('#tailnet-link').getAttribute('href'),'http://100.64.0.10/');
  status={...status,phase:'memory_paused',connected:false,policy_ready:false};
  await page.evaluate(()=>EcoTailnet.load());await page.locator('#tailnet-link').waitFor({state:'hidden'});
  assert.match(await page.locator('#tailnet-status').innerText(),/Panel lokalny nadal działa/);
  fail=true;await page.locator('#tailnet-name').fill('nowa-nazwa');await page.locator('#tailnet-save').click();
  await page.waitForFunction(()=>document.querySelector('#tailnet-message').textContent.includes('nie powiódł'));
  assert.equal(await page.locator('#tailnet-name').inputValue(),'nowa-nazwa');
  assert.equal(await page.locator('#tailnet-cancel').isVisible(),true);
  await page.locator('#tailnet-cancel').click();
  await page.locator('.tailnet-settings').scrollIntoViewIfNeeded();
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.screenshot({path:'test-results/tailnet-'+width+'.png'});
  await page.close();console.log('PASS Tailnet '+width+'px: default off, polling preserves edits, save clears key, failure keeps draft, no stale link');
 }
 assert.deepEqual(errors,[]);
 assert.ok(writes.every(path=>['/api/tailnet','/api/panel/clock'].includes(path)));
 console.log('PASS: no boiler or pump write requests');
}finally{await browser.close();await new Promise(done=>server.close(done));}
