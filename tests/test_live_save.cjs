const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const {signal}=require('../src/panel_web/wifi_signal.js');
const app=fs.readFileSync('src/panel_web/app.js','utf8');
const start=app.indexOf('async function poll(){');
const flow=app.slice(start,app.indexOf('for(const button of document.querySelectorAll',start));
function context(request){const ctx={actionBusy:false,actionEpoch:0,state:{write_transport:'rs485'},noticeKind:'',csrf:'c',serverReachable:true,render(){},notice(){},setTimeout:fn=>fn(),request};vm.runInNewContext(flow,ctx);return ctx;}
test('only a known pre-dispatch busy rejection may be retried',async()=>{
 let posts=0;const ctx=context(async(path,payload)=>{
  if(payload){if(++posts===1)throw Object.assign(Error('busy'),{busy:true});return {job:{id:'own'}};}
  return {job:{id:'own',status:'done',result:{state:'verified'}}};
 });
 assert.equal((await ctx.run('/api/write/save',{id:'1281',value:42,confirm:true})).state,'verified');assert.equal(posts,2);
});
test('ambiguous save response and ordinary conflicts are never retried',async()=>{
 for(const error of [Error('timeout'),Object.assign(Error('range'),{status:409})]){
  let posts=0;const ctx=context(async()=>{posts++;throw error;});
  await assert.rejects(ctx.run('/api/write/save',{id:'1281',value:42,confirm:true}));assert.equal(posts,1);
 }
});
test('visible and hidden tabs keep polling without a visibility pause',async()=>{
 let calls=0;const ctx=context(async()=>{calls++;return {fresh:true};});Object.assign(ctx,{document:{hidden:true},pollBusy:false});
 await ctx.poll();assert.equal(calls,1);ctx.document.hidden=false;await ctx.poll();assert.equal(calls,2);
 ctx.document.hidden=true;await ctx.poll();assert.equal(calls,3);assert.equal(ctx.serverReachable,true);
 assert.doesNotMatch(app,/if\(document\.hidden\|\|/);
 assert.match(app,/document\.addEventListener\('visibilitychange',poll\)/);
 assert.match(app,/setInterval\(poll,2000\)/);
});

test('background polling does not overlap requests and recovers after a read failure',async()=>{
 let finish,calls=0;const ctx=context(()=>{calls++;return new Promise(resolve=>{finish=resolve;});});
 Object.assign(ctx,{document:{hidden:true},pollBusy:false});
 const pending=ctx.poll();await ctx.poll();assert.equal(calls,1);finish({fresh:true});await pending;
 assert.equal(ctx.pollBusy,false);
 ctx.request=async()=>{throw Error('temporary outage');};await ctx.poll();
 assert.equal(ctx.serverReachable,false);assert.equal(ctx.pollBusy,false);
 ctx.request=async()=>({fresh:true});await ctx.poll();assert.equal(ctx.serverReachable,true);
});
test('five bars and percentage use ESP RSSI, never boiler or ecoNET status',()=>{
 for(const [rssi,percent,bars]of [[-105,0,0],[-100,0,0],[-90,20,1],[-80,40,2],[-70,60,3],[-60,80,4],[-50,100,5],[-30,100,5]]){
  const v=signal({connected:true,rssi_dbm:rssi});assert.equal(v.percent,percent);assert.equal(v.bars,bars);
 }
 for(const n of [null,{}, {connected:false,rssi_dbm:-50},{connected:true,rssi_dbm:null}])assert.equal(signal(n).percent,null);
 assert.equal(signal({connected:true,rssi_dbm:-50},false).percent,null);
});
test('atomic backend refreshes before sending, retains bounds, ownership and verification',()=>{
 const cpp=fs.readFileSync('src/panel.cpp','utf8'),body=cpp.slice(cpp.indexOf('J *save_setting('),cpp.indexOf('bool control_observed('));
 assert.match(body,/prepare\(args.p,owner\)/);assert.match(body,/execute\(args.p,owner,prepared.p\)/);
 assert.match(body,/schedule:/);assert.match(body,/DeleteItemFromObjectCaseSensitive\(args.p,"before"\)/);
 assert.match(cpp,/owner!=ticket_owner/);assert.match(cpp,/generic_value\(p,num\(get\(t,"value"\)\)\)/);
 assert.match(cpp,/while\(cJSON_GetArraySize\(history\)>6\)/);assert.doesNotMatch(cpp,/put\(data,"completed_jobs"/);
});
