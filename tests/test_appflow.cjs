const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const {ticketMatches,isThermostatMenu}=require('../src/panel_web/mirror.js');
const source=fs.readFileSync(require.resolve('../src/panel_web/app.js'),'utf8');
const flowStart=source.indexOf('async function poll(){');
const flow=source.slice(flowStart,source.indexOf('for(const button of document.querySelectorAll',flowStart));
const target={id:'1287',before:43,min:40,max:65,step:1};
function context(overrides={}){
 return {csrf:'csrf',state:{targets:{'1287':target},targets_fresh:true,write_transport:'econet'},actionBusy:false,
  actionEpoch:0,pollBusy:false,savingTemperature:false,serverReachable:true,fresh:()=>true,render:()=>{},notice:()=>{},noticeKind:'connection',
  setTimeout:fn=>fn(),document:{hidden:false},poll:async()=>{},EcoMirror:{ticketMatches},...overrides};
}
test('one Save submits one atomic operation without a second dialog',async()=>{
 const calls=[],prepared={...target,transport:'econet',expires:Date.now()/1000+60,token:'t'};
 const ctx=context();vm.runInNewContext(flow,ctx);
 ctx.run=async(path,payload)=>{calls.push({path,payload});return path.endsWith('prepare')?prepared:{state:'verified',message:'OK'};};
 let closed=0;await ctx.saveTemperature('1287',44,()=>{closed++;});
 assert.equal(calls.length,1);assert.equal(calls[0].path,'/api/write/save');
 assert.equal(calls[0].payload.value,44);assert.equal(calls[0].payload.confirm,true);assert.equal(closed,1);
 assert.equal('before' in calls[0].payload,false);
 assert.equal(ctx.savingTemperature,false);
});
test('external baseline changes do not block an absolute Save, but offline errors remain real',async()=>{
 const ctx=context();vm.runInNewContext(flow,ctx);ctx.state.targets['1287']={...target,before:45};
 ctx.run=async(path,payload)=>{assert.equal(path,'/api/write/save');assert.equal(payload.value,44);return {state:'verified'};};
 await ctx.saveTemperature('1287',44);assert.equal(ctx.savingTemperature,false);
 ctx.run=async()=>{throw Error('offline');};await assert.rejects(ctx.saveTemperature('1287',44));
 assert.equal(ctx.savingTemperature,false);
});
test('RS485 room Save keeps the native route and sends one command',async()=>{
 const id='STER_TEMP_DAY_1',room={id,before:22.8,min:10,max:35,step:0.1,legacy:true,
  profile_mode:2,profile_raw:2,wire_index:10,parameter_count:12};
 const prepared={...room,transport:'rs485',expires:Date.now()/1000+60,token:'t'},calls=[];
 const ctx=context({state:{targets:{[id]:room},targets_fresh:true,write_transport:'rs485'}});vm.runInNewContext(flow,ctx);
 ctx.run=async(path,payload)=>{calls.push({path,payload});return path.endsWith('prepare')?prepared:{state:'verified'};};
 await ctx.saveTemperature(id,22.9);
 assert.deepEqual(JSON.parse(JSON.stringify(calls[0].payload)),{id,value:22.9,confirm:true});
 assert.equal(calls.length,1);assert.equal(calls[0].path,'/api/write/save');
});
test('RS485 room ticket rejects a changed raw mode or parameter layout',()=>{
 const id='STER_TEMP_DAY_1',room={id,before:22.8,min:10,max:35,step:0.1,legacy:true,
  profile_mode:2,profile_raw:2,wire_index:10,parameter_count:12};
 const ticket={...room,transport:'rs485',expires:Date.now()/1000+60};
 const state={targets:{[id]:room},targets_fresh:true,write_transport:'rs485'};
 assert.equal(ticketMatches(state,ticket,true),true);
 for(const key of ['profile_mode','profile_raw','wire_index','parameter_count']) {
  assert.equal(ticketMatches({...state,targets:{[id]:{...room,[key]:room[key]+1}}},ticket,true),false);
 }
 assert.equal(ticketMatches({...state,write_transport:'econet'},ticket,true),false);
});

test('inactive Party Save changes the stored target through RS485, not the active profile',async()=>{
 const id='STER_TEMP_SET_PARTY_1',room={id,before:23.1,min:10,max:35,step:0.1,legacy:true,
  active_profile:false,active_target:22,profile_mode:2,profile_raw:2,wire_index:2,parameter_count:12};
 const ticket={...room,transport:'rs485',expires:Date.now()/1000+60,token:'t'},calls=[];
 const snapshot={targets:{[id]:room},targets_fresh:true,write_transport:'rs485'};
 assert(ticketMatches(snapshot,ticket,true));
 assert.equal(ticketMatches({...snapshot,targets:{[id]:{...room,active_target:23}}},ticket,true),false);
 const ctx=context({state:snapshot});vm.runInNewContext(flow,ctx);
 ctx.run=async(path,payload)=>{calls.push({path,payload});return path.endsWith('prepare')?ticket:{state:'verified'};};
 await ctx.saveTemperature(id,23.2);
 assert.equal(calls.length,1);assert.equal(calls[0].payload.id,id);assert.equal(calls[0].payload.value,23.2);
 assert.equal(calls[0].path,'/api/write/save');
 assert.equal(calls.some(call=>call.path.includes('/control')||call.payload.id==='STER_MODE_1'),false);
});

test('room tiles use native Party and holiday targets only for the native route',()=>{
 const {targetIdForTile}=require('../src/panel_web/mirror.js');
 for(const [mode,id]of [[5,'STER_TEMP_SET_PARTY_1'],[6,'STER_TEMP_SET_SUMMER_1']]){
  assert.equal(targetIdForTile('ecoSterTemp1',{write_transport:'rs485',curr:{ecoSterMode1:mode|128}}),id);
  assert.equal(targetIdForTile('ecoSterTemp1',{write_transport:'econet',curr:{ecoSterMode1:mode}}),'legacy:'+id);
 }
});
test('room menu includes RS485-backed entries as well as ecoNET entries',()=>{
 assert.equal(isThermostatMenu({source:'RS485_THERMOSTAT_PARAMETERS_220'}),true);
 assert.equal(isThermostatMenu({source:'editParams'}),true);
 assert.equal(isThermostatMenu({source:'rmParamsData'}),false);
});
test('job monitoring recovers GET failures without repeating POST',async()=>{
 let posts=0,gets=0;
 const ctx=context({request:async(path,payload)=>{
  if(payload){posts++;return {job:{id:'j'}};}
  gets++;if(gets===1)throw Error('temporary outage');
  return {job:{id:'j',status:'done',result:{state:'verified'}}};
 }});vm.runInNewContext(flow,ctx);
 assert.equal((await ctx.run('/api/write/execute',{token:'t'})).state,'verified');
 assert.equal(posts,1);assert.equal(gets,3);assert.equal(ctx.serverReachable,true);assert.equal(ctx.actionBusy,false);
});
test('unknown POST outcome is never retried',async()=>{
 let calls=0;const ctx=context({request:async()=>{calls++;throw Error('response lost');}});vm.runInNewContext(flow,ctx);
 await assert.rejects(ctx.run('/api/write/execute',{token:'t'}));assert.equal(calls,1);assert.equal(ctx.actionBusy,false);
});
test('replaced job is not treated as this command success',async()=>{
 const ctx=context({request:async(path,payload)=>payload?{job:{id:'j'}}:{job:{id:'other',status:'done'}}});vm.runInNewContext(flow,ctx);
 await assert.rejects(ctx.run('/api/write/execute',{token:'t'}));
});
test('HTTP 409 from small job endpoint ends monitoring without another POST',async()=>{
 let posts=0,gets=0;const ctx=context({request:async(path,payload)=>{if(payload){posts++;return {job:{id:'j'}};}gets++;const error=Error('replaced');error.status=409;throw error;}});
 vm.runInNewContext(flow,ctx);await assert.rejects(ctx.run('/api/write/execute',{token:'t'}));assert.equal(posts,1);assert.equal(gets,1);
});
test('confirmed result survives failure of the final full-panel GET',async()=>{
 const calls=[],ctx=context({request:async(path,payload)=>{calls.push(path);if(payload)return {job:{id:'j'}};if(path.startsWith('/api/job?'))return {job:{id:'j',status:'done',result:{state:'verified'}}};throw Error('read failed');}});
 vm.runInNewContext(flow,ctx);assert.equal((await ctx.run('/api/write/execute',{token:'t'})).state,'verified');
 assert.equal(ctx.serverReachable,false);assert.deepEqual(calls,['/api/write/execute','/api/job?id=j','/api/state']);
});
test('a readable target marked write-blocked never prepares or dispatches',async()=>{
 let calls=0;const ctx=context({state:{targets:{'1287':{...target,write_ready:false,write_error:'catalogue changed'}},targets_fresh:true},request:async()=>{calls++;}});
 vm.runInNewContext(flow,ctx);await assert.rejects(ctx.saveTemperature('1287',44));assert.equal(calls,0);
});
test('poll recovers missing session and network failure on next cycle',async()=>{
 let cycles=0;const ctx=context({csrf:'',request:async path=>{
  if(path==='/api/session')return {csrf:'recovered'};
  if(++cycles===1)throw Error('offline');return {fresh:true};
 }});vm.runInNewContext(flow,ctx);
 await ctx.poll();assert.equal(ctx.serverReachable,false);assert.equal(ctx.pollBusy,false);
 await ctx.poll();assert.equal(ctx.serverReachable,true);assert.equal(ctx.csrf,'recovered');assert.equal(ctx.pollBusy,false);
});
test('startup polling is installed even when first handshake fails',async()=>{
 const boot=source.slice(source.indexOf('async function bootPanel(){'),source.indexOf("$('esp-login-form').onsubmit="));
 let timers=0;const ctx=context({espPollTimer:null,setInterval:()=>++timers,request:async()=>{throw Error('offline');}});
 vm.runInNewContext(boot,ctx);await assert.rejects(ctx.bootPanel());assert.equal(timers,1);assert.equal(ctx.espPollTimer,1);
});
test('native RS485 login never asks to configure the unused ecoNET route',async()=>{
 const start=source.indexOf('async function bootPanel(){'),end=source.indexOf("$('esp-login-form')",start);
 const boot=source.slice(start,end);
 for(const route of ['rs485','econet','hybrid']){
  let prompts=0;
  const ctx={espPollTimer:1,setInterval:()=>1,poll:async()=>{},request:async()=>({csrf:'c'}),csrf:'',
   state:{requires_configuration:true,write_transport:route},$:(id)=>id==='esp-login'?{close:()=>{}}:{showModal:()=>{prompts++;}}};
  vm.runInNewContext(boot,ctx);await ctx.bootPanel();assert.equal(prompts,route==='rs485'?0:1);
 }
});

test('poll and job monitoring adopt the new boot nonce without repeating commands',async()=>{
 const pollContext=context({request:async()=>({csrf:'new-boot'})});
 vm.runInNewContext(flow,pollContext);await pollContext.poll();assert.equal(pollContext.csrf,'new-boot');
 let posts=0;
 const jobContext=context({request:async(path,payload)=>{
  if(payload){posts++;return {job:{id:'j'}};}
  return {csrf:'new-boot',job:{id:'j',status:'done',result:{state:'verified'}}};
 }});
 vm.runInNewContext(flow,jobContext);await jobContext.run('/api/write/execute',{token:'t'});
 assert.equal(jobContext.csrf,'new-boot');assert.equal(posts,1);
});

const loginFlow=source.slice(source.indexOf("$('esp-login-form').onsubmit="),source.indexOf("$('esp-login').addEventListener"));
function loginContext(remember,ok=true){
 const fields={'esp-login-form':{},'esp-user':{value:'user'},'esp-password':{value:'password'},
  'esp-remember':{checked:remember}},calls=[];
 const ctx={$:id=>fields[id],btoa:s=>Buffer.from(s).toString('base64'),
  fetch:async(path,options)=>{calls.push({path,options});return {ok,json:async()=>({message:'Not saved'})};},
  bootPanel:async()=>{calls.push({boot:true});},notice:()=>{},text:(id,value)=>{calls.push({id,value});}};
 vm.runInNewContext(loginFlow,ctx);return {fields,calls};
}
test('login sends the checkbox preference and clears the password after acceptance',async()=>{
 for(const remember of [true,false]){
  const {fields,calls}=loginContext(remember),submitter={disabled:false};
  await fields['esp-login-form'].onsubmit({preventDefault:()=>{},submitter});
  assert.equal(calls[0].options.headers['X-Lazar-Remember'],remember?'1':'0');
  assert.equal(fields['esp-password'].value,'');assert.equal(calls[1].boot,true);assert.equal(submitter.disabled,false);
 }
 assert.doesNotMatch(loginFlow,/localStorage|sessionStorage/);
});
test('failed remembered login reports the persistence error and never boots the panel',async()=>{
 const {fields,calls}=loginContext(true,false),submitter={disabled:false};
 await fields['esp-login-form'].onsubmit({preventDefault:()=>{},submitter});
 assert.equal(calls[1].value,'Not saved');assert.equal(calls.some(call=>call.boot),false);
 assert.equal(submitter.disabled,false);
});
