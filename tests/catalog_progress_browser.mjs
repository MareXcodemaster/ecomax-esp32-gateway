// Loopback only: catalogue progress must remain visible while the tree is empty.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {chromium} from 'playwright';
const root=resolve('src/panel_web'),errors=[];
const version=JSON.parse(await readFile('package.json','utf8')).version;
const state={version,csrf:'demo',fresh:true,connected:true,busy:false,error:'',targets_fresh:true,write_transport:'rs485',requires_configuration:false,
 system:{controllerID:'ecoMAX860P3-O DEMO'},values:{},names:{},menu:[],enums:{},units:[],structure:[],category_names:[],tiles:[],endpoints:{},versions:{},tests:[],
 curr:{mode:0},targets:{},controls:{},econet_health:{fresh:true,api_ok:false},schedules:{ecomaxSchedules:{}},pellet:{},
 profile_status:'Pobieranie katalogu',menu_read_error:'Pobieranie katalogu RS485: etap 4/11, nazwy 24/198'};
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  if(req.method!=='GET'&&!['/api/session','/api/updates/check','/api/panel/clock'].includes(url.pathname))throw new Error('Unexpected POST '+url.pathname);
  const result=url.pathname==='/api/session'?{csrf:'demo'}:url.pathname==='/api/state'?state:url.pathname==='/api/updates'?{current:version,phase:'up_to_date'}:url.pathname==='/api/job'?{job:null,busy:false}:{};
  res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify(result));return;
 }
 const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
 if(!path.startsWith(root+sep)){res.writeHead(403).end();return;}
 try{res.writeHead(200,{'Content-Type':{'.html':'text/html','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml'}[extname(path)]||'application/octet-stream'}).end(await readFile(path));}
 catch{res.writeHead(404).end();}
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});
try{
 const page=await browser.newPage({viewport:{width:390,height:844}});page.on('pageerror',e=>errors.push(e.message));
 await page.goto('http://127.0.0.1:'+server.address().port+'/#service');
 await page.locator('#service-catalog-status').waitFor();
 await page.evaluate(()=>{clearInterval(espPollTimer);espPollTimer=null;});await page.waitForFunction(()=>!pollBusy);
 for(const kind of ['service','user']){
  await page.evaluate(kind=>selectView(kind),kind);
  state.menu_read_error='Pobieranie katalogu RS485: etap 4/11, nazwy 24/198';await page.evaluate(()=>poll());
  assert.match(await page.locator('#'+kind+'-catalog-status').textContent(),/24\/198/);
  state.menu_read_error='Pobieranie katalogu RS485: etap 4/11, nazwy 32/198';await page.evaluate(()=>poll());
  assert.match(await page.locator('#'+kind+'-catalog-status').textContent(),/32\/198/);
  state.menu_read_error='';state.profile_status='Katalog kompletny';await page.evaluate(()=>poll());
  assert.equal(await page.locator('#'+kind+'-catalog-status').isVisible(),false);
 }
 assert.deepEqual(errors,[]);console.log('PASS catalogue progress: empty user/service tree, changing progress, hidden after success, no setting writes');
}finally{await browser.close();await new Promise(done=>server.close(done));}
