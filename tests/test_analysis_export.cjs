const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const {snapshot,settingEvent,collect}=require('../src/panel_web/analysis_export.js');
const base=()=>({version:'demo',fresh:true,system:{controllerID:'ecoMAX DEMO',moduleASoftVer:'test',modulePanelSoftVer:'test',uid:'PRIVATE-UID'},curr:{mode:3,tempCO:0,lambdaLevel:73,pumpCOWorks:false},targets:{1281:{name:'CWU',before:45,min:35,max:65}},menu:[{index:12,name:'Nadmuch',unit_name:'%',metadata:{value:32,minv:10,maxv:100}}],csrf:'PRIVATE-TOKEN',username:'PRIVATE-LOGIN',esp_network:{ssid:'PRIVATE-SSID'},host:'PRIVATE-IP',tests:[{token:'PRIVATE-TICKET'}]});
const columns=['time','native','mode','co','oxygen','gapBefore'];
function reader({mutate,fail}={}){let exports=0;const calls=[];const read=async path=>{
 calls.push(path);if(path==='/api/export'){const value=base();if(++exports===2&&mutate)mutate(value);return value;}
 if(fail)throw Error('network');const q=new URL(path,'http://localhost').searchParams,from=Number(q.get('from')),to=Number(q.get('to'));
 assert.equal(q.get('raw'),'1');assert.ok(to-from<=3600);
 const rows=[];for(let t=from;t<=to;t+=60)rows.push([t,1,3,0,7.3,0]);
 return {export_raw:true,columns,rows,archive:{ready:true,interval_seconds:60}};
 };return {read,calls};}

test('analysis snapshot includes known measurements and settings but no credentials or identity',()=>{
 const input=base(),out=snapshot(input);assert.equal(out.readings.co,0);assert.equal(out.readings.pumpCO,0);assert.equal(out.readings.oxygen,7.3);assert.equal(out.readings.flow,null);
 assert.equal(out.parameters[0].value,32);assert.equal(out.setpoints['1281'].value,45);
 assert.doesNotMatch(JSON.stringify(out),/PRIVATE|csrf|password|username|esp_network|uid/);
 assert.equal(input.curr.lambdaLevel,73);assert.equal(input.system.uid,'PRIVATE-UID');
});

test('three days are fetched as 72 bounded read-only pages without further downsampling',async()=>{
 const {read,calls}=reader(),from=1700000000,to=from+259200,progress=[];
 const report=await collect(read,{from,to,onProgress:(n,total)=>progress.push([n,total])});
 assert.equal(calls.filter(p=>p.startsWith('/api/history?')).length,72);assert.equal(calls.filter(p=>p==='/api/export').length,2);
 assert.equal(report.history.sample_count,4321);assert.equal(report.history.additional_downsampling,false);assert.equal(report.history.rows[0][0],from);assert.equal(report.history.rows.at(-1)[0],to);
 assert.deepEqual(progress.at(-1),[72,72]);assert.doesNotMatch(JSON.stringify(report),/PRIVATE/);
 assert.equal(report.history.units.oxygen,'%');assert.equal(report.history.rows[0][3],0);
});

test('short mode transitions, null readings and page-boundary gaps are preserved',async()=>{
 const from=1700000000;let pages=0;const {read:normal}=reader();
 const read=async path=>path==='/api/export'?normal(path):{export_raw:true,columns,rows:++pages===1?[[from,1,2,null,7.3,0],[from+2,1,3,0,null,0]]:[[from+3601,0,7,null,null,0]],archive:{ready:false,dropped:1}};
 const report=await collect(read,{from,to:from+7200});assert.equal(report.history.rows.length,3);assert.equal(report.history.rows[1][0],from+2);assert.equal(report.history.rows[1][4],null);assert.equal(report.history.rows[2].at(-1),1);
 assert.ok(report.warnings.some(w=>w.includes('przerwy')));assert.ok(report.warnings.some(w=>w.includes('RAM')));
});

test('bad windows, malformed data, old firmware and failed pages do not produce an export',async()=>{
 const from=1700000000,{read}=reader();for(const to of [from,from-1,from+259201,NaN])await assert.rejects(collect(read,{from,to}));
 await assert.rejects(collect(reader({fail:true}).read,{from,to:from+3600}),/network/);
 for(const page of [{columns,rows:[]},{export_raw:true,columns,rows:[[from-1,1,3,0,null,0]]},{export_raw:true,columns,rows:[[from,1,3,'bad',null,0]]},{export_raw:true,columns:['time','time'],rows:[]}]){
  await assert.rejects(collect(path=>path==='/api/export'?Promise.resolve(base()):Promise.resolve(page),{from,to:from+3600}));
 }
 await assert.rejects(collect(reader({mutate:value=>{value.version='changed';}}).read,{from,to:from+3600}),/firmware/);
});

test('cancel stops further pages; empty and stale data are explicitly marked',async()=>{
 const controller=new AbortController(),from=1700000000,{read,calls}=reader();controller.abort();
 await assert.rejects(collect(read,{from,to:from+3600,signal:controller.signal}),{name:'AbortError'});assert.equal(calls.length,0);
 const report=await collect(async path=>path==='/api/export'?{...base(),fresh:false}:{export_raw:true,columns,rows:[],archive:{ready:false}},{from,to:from+3600});
 assert.equal(report.history.sample_count,0);assert.ok(report.warnings.length>=3);
});

test('shared request cancellation aborts a GET and always clears its timeout',async()=>{
 const app=fs.readFileSync('src/panel_web/app.js','utf8'),source=app.slice(app.indexOf('async function request('),app.indexOf('function selectView('));let cleared=0;
 const ctx={AbortController,setTimeout:()=>1,clearTimeout:()=>{cleared++;},fetch:(path,options)=>new Promise((resolve,reject)=>{options.signal.addEventListener('abort',()=>reject(Object.assign(Error('cancelled'),{name:'AbortError'})),{once:true});})};
 vm.runInNewContext(source,ctx);const controller=new AbortController(),pending=ctx.request('/api/history',undefined,controller.signal);controller.abort();await assert.rejects(pending,{name:'AbortError'});assert.equal(cleared,1);
});

test('raw history retains sub-ten-second events and bounds each response to an hour',()=>{
 const cpp=fs.readFileSync('src/panel_history.cpp','utf8'),api=fs.readFileSync('src/api.cpp','utf8');
 assert.match(cpp,/raw&&to-from>3600/);assert.match(cpp,/if\(raw\)\{size=unique/);assert.match(cpp,/else size=history_data::reduce/);
 assert.match(api,/if\(!authorize\(req\)\)return ESP_OK;[\s\S]*?raw=query_number\(req,"raw",0\)/);
 assert.match(api,/raw&&\(!from\|\|!to\|\|to-from>3600\)/);
});

test('settings events reject malformed values and whitelist fields',()=>{
 const event={schema:1,kind:'baseline',time:1700000000,event_id:'1699999200:0',values:{'menu:12':0,'target:room':null},password:'PRIVATE',uid:'PRIVATE'};
 const out=settingEvent(event);assert.equal(out.values['menu:12'],0);assert.equal(out.values['target:room'],null);assert.doesNotMatch(JSON.stringify(out),/PRIVATE|password|uid/);
 for(const bad of [{...event,schema:2},{...event,kind:'guess'},{...event,event_id:'bad'},{...event,values:{password:123}},{...event,values:{'menu:12':'32'}}])assert.throws(()=>settingEvent(bad));
});

test('settings timeline preserves preceding baseline, de-duplicates page boundaries and reports missing periods',async()=>{
 const from=1700000000,hour=Math.floor(from/3600)*3600,{read:normal}=reader();
 const events=[{schema:1,kind:'baseline',time:hour,event_id:hour+':0',values:{'menu:12':32}},
  {schema:1,kind:'changes',time:from+1,event_id:hour+':100',values:{'menu:12':33}},
  {schema:1,kind:'baseline',time:hour+3600,event_id:(hour+3600)+':0',values:{'menu:12':33}}];
 const read=async path=>{const page=await normal(path);if(path==='/api/export')return page;const q=new URL(path,'http://localhost').searchParams,start=Number(q.get('from')),end=Number(q.get('to'));
  return {...page,settings_history:{ready:true,dropped:1,pending:0,bad_files:0,events:events.filter(e=>e.time>=Math.floor(start/3600)*3600&&e.time<=end)}};};
 const report=await collect(read,{from,to:from+7200});assert.equal(report.settings_history.event_count,3);assert.equal(report.settings_history.events[0].time,hour);
 assert.equal(report.settings_history.events[1].values['menu:12'],33);assert.equal(report.settings_history.historical_backfill,false);
 assert.equal(report.settings_history.dictionary['menu:12'].name,'Nadmuch');assert.equal(report.settings_history.quality.dropped,1);
 assert.ok(report.warnings.some(w=>w.includes('okresy bez')));assert.ok(report.warnings.some(w=>w.includes('pominiete/uszkodzone')));
 assert.doesNotMatch(JSON.stringify(report),/PRIVATE/);
});
