// Navigation and reload tests use only loopback and synthetic read-only data.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';

const root=resolve('src/panel_web'),version=JSON.parse(await readFile('package.json','utf8')).version;
const requests=[],errors=[];
const state={version,csrf:'navigation-demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX DEMO'},values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],alarms:[],
 curr:{mode:0},targets:{},controls:{},econet_health:{fresh:true,api_ok:false},esp_network:{connected:true,rssi_dbm:-65,hostname:'piec'},schedules:{ecomaxSchedules:{}},pellet:{}};
let requireLogin=false,loggedIn=false,failedStateReads=0;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');requests.push([req.method,url.pathname]);
 if(url.pathname.startsWith('/api/')){
  let value;
  if(url.pathname==='/api/session'){
   if(requireLogin&&!loggedIn){res.writeHead(401,{'Content-Type':'application/json'}).end('{"message":"DEMO login"}');return;}
   value={csrf:state.csrf};
  }else if(url.pathname==='/api/login'){loggedIn=true;value={ok:true};}
  else if(url.pathname==='/api/state'){
   if(failedStateReads>0){failedStateReads--;res.writeHead(503,{'Content-Type':'application/json'}).end('{"message":"DEMO offline"}');return;}
   value=state;
  }else if(url.pathname==='/api/panel/clock')value={ok:true};
  else if(url.pathname==='/api/updates')value={current:version,current_release:version,version,phase:'up_to_date',automatic:false};
  else if(url.pathname==='/api/history')value={schema:1,columns:['time','native','mode','co','coSet','cwu','cwuSet','room','roomSet'],rows:[],archive:{ready:true,samples:0}};
  res.writeHead(value?200:404,{'Content-Type':'application/json'}).end(JSON.stringify(value||{error:'DEMO endpoint disabled'}));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{const mime={'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.png':'image/png','.ico':'image/x-icon','.jpg':'image/jpeg','.woff2':'font/woff2'};res.writeHead(200,{'Content-Type':mime[extname(path)]||'application/octet-stream'}).end(await readFile(path));}catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port,browser=await chromium.launch({headless:true});
await mkdir('test-results',{recursive:true});
try{
 for(const width of [390,1280]){
  const page=await browser.newPage({viewport:{width,height:900}});page.on('pageerror',error=>errors.push(error.message));
  await page.goto(origin);await page.waitForFunction(()=>document.querySelector('#version').textContent.includes('beta'));
  const keys=await page.locator('[data-section]').evaluateAll(nodes=>nodes.map(node=>node.dataset.section));
  for(const key of keys){
   await page.evaluate(key=>selectView(key),key);
   assert.equal(new URL(page.url()).hash,key==='live'?'':'#'+key);
   await page.reload();await page.waitForFunction(key=>document.body.dataset.view===key,key);
   assert.equal(await page.locator(`[data-section="${key}"]`).isVisible(),true);
   assert.equal(await page.locator('main > section[data-section]:visible').count(),1);
   if(['data','menu','api','tests'].includes(key))assert.equal(await page.locator('#local-diagnostics').getAttribute('open'),'');
  }
  await page.evaluate(()=>selectView('alarms'));await page.evaluate(()=>selectView('pellet'));
  await page.goBack();await page.waitForFunction(()=>document.body.dataset.view==='alarms');
  await page.goForward();await page.waitForFunction(()=>document.body.dataset.view==='pellet');
  await page.goto(origin+'/#advanced');await page.waitForFunction(()=>document.body.dataset.view==='advanced');
  await page.goto(origin+'/?demo=1#unknown');await page.waitForFunction(()=>document.body.dataset.view==='live');
  assert.equal(new URL(page.url()).hash,'');assert.equal(new URL(page.url()).search,'?demo=1');
  if(width<900)await page.locator('#nav-toggle').click();
  await page.locator('#main-nav [data-view="alarms"]').click();
  assert.equal(new URL(page.url()).hash,'#alarms');
  await page.reload();await page.waitForFunction(()=>document.body.dataset.view==='alarms');
  await page.screenshot({path:`test-results/navigation-alarms-${width}.png`});await page.close();
  console.log(`PASS navigation ${width}px: ${keys.length} views survive reload, diagnostics disclosure, deep link, back/forward, unknown route`);
 }
 const first=await browser.newPage(),second=await browser.newPage();
 await first.goto(origin+'/#alarms');await second.goto(origin+'/#pellet');
 await first.reload();await second.reload();
 assert.equal(await first.locator('body').getAttribute('data-view'),'alarms');assert.equal(await second.locator('body').getAttribute('data-view'),'pellet');
 await first.close();await second.close();
 requireLogin=true;const login=await browser.newPage();login.on('pageerror',error=>errors.push(error.message));
 await login.goto(origin+'/#alarms');await login.locator('#esp-login[open]').waitFor();
 await login.locator('#esp-user').fill('admin');await login.locator('#esp-password').fill('demo');await login.locator('#esp-login-form button[type=submit]').click();
 await login.waitForFunction(()=>!document.querySelector('#esp-login').open&&document.body.dataset.view==='alarms');await login.close();requireLogin=false;
 failedStateReads=1;const retry=await browser.newPage();retry.on('pageerror',error=>errors.push(error.message));
 await retry.goto(origin+'/#settings');await retry.waitForFunction(()=>state?.fresh===true&&document.body.dataset.view==='settings');await retry.close();
 assert.deepEqual(errors,[]);
 assert.deepEqual(requests.filter(([method,path])=>method==='POST'&&!['/api/panel/clock','/api/login'].includes(path)),[],'restoring or navigating never submits boiler writes');
 console.log('PASS independent tabs, login and temporary read failure preserve the URL view; no boiler writes');
}finally{await browser.close();await new Promise(done=>server.close(done));}
