// Loopback-only integration: the unchanged START button exposes the one-shot schedule confirmation.
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
state.curr.mode=5;state.controls={start:true,stop:true,mode:5,transport:'rs485',schedule_until:'10.11.2026 01:30'};
let job=null,action='',prepared=0,executed=0;
const server=createServer(async(req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname.startsWith('/api/')){
  if(req.method==='POST'&&url.pathname.startsWith('/api/control/')){
   let body='';for await(const chunk of req)body+=chunk;const payload=JSON.parse(body);
   if(url.pathname==='/api/control/prepare'){
    assert.ok(['start','stop'].includes(payload.action));action=payload.action;prepared++;
    const result={action,token:'a'.repeat(32),expires:Date.now()/1000+60,before_mode:state.curr.mode,transport:'rs485'};
    if(action==='start')result.schedule_run={until:'10.11.2026 01:30'};
    job={id:'job-'+prepared,status:'done',result};
   }else{
    assert.equal(payload.token,'a'.repeat(32));assert.equal(payload.confirm,true);executed++;
    state.schedule_run={active:action==='start',until:'10.11.2026 01:30',message:'Potwierdzony okres pracy'};
    state.controls.start=false;if(action==='stop')state.curr.mode=state.controls.mode=7;
    job={id:'execute-'+executed,status:'done',result:{state:'verified',message:'Potwierdzono'}};
   }
   res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify({job:{id:job.id}}));return;
  }
  if(req.method!=='GET'&&!['/api/session','/api/updates/check','/api/panel/clock'].includes(url.pathname))throw new Error('Unexpected POST '+url.pathname);
  const result=url.pathname==='/api/session'?{csrf:'demo'}:url.pathname==='/api/state'?state:url.pathname==='/api/updates'?{current:version,phase:'up_to_date'}:url.pathname==='/api/job'?{job,busy:false}:{};
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
 await page.goto('http://127.0.0.1:'+server.address().port+'/');
 await page.waitForFunction(()=>state?.controls?.start===true);
 await page.evaluate(()=>{clearInterval(espPollTimer);espPollTimer=null;});await page.waitForFunction(()=>!pollBusy);
 assert.equal((await page.locator('#boiler-start').textContent()).trim(),'Włącz kocioł');
 assert.equal(await page.locator('#boiler-start').isEnabled(),true);
 await page.locator('#boiler-start').click();await page.locator('#control-dialog').waitFor({state:'visible'});
 assert.match(await page.locator('#control-description').textContent(),/10\.11\.2026 01:30/);
 assert.equal(await page.locator('#control-submit').isEnabled(),false);
 await page.locator('#control-cancel').click();assert.equal(executed,0);
 await page.locator('#boiler-start').click();await page.locator('#control-dialog').waitFor({state:'visible'});
 await page.locator('#control-checkbox').check();await page.locator('#control-submit').click();
 await page.waitForFunction(()=>!actionBusy&&state?.schedule_run?.active);
 assert.equal(executed,1);assert.equal(await page.locator('#boiler-start').isEnabled(),false);
 assert.match(await page.locator('#control-schedule').textContent(),/01:30/);
 await page.locator('#boiler-stop').click();await page.locator('#control-dialog').waitFor({state:'visible'});
 assert.doesNotMatch(await page.locator('#control-description').textContent(),/01:30/);
 await page.locator('#control-checkbox').check();await page.locator('#control-submit').click();
 await page.waitForFunction(()=>!actionBusy&&!state?.schedule_run?.active);
 assert.equal(executed,2);assert.equal(await page.locator('#control-schedule').isVisible(),false);
 assert.deepEqual(errors,[]);console.log('PASS existing START label, deadline confirmation, cancel without command, one dispatch, active status and STOP on mobile');
}finally{await browser.close();await new Promise(done=>server.close(done));}
