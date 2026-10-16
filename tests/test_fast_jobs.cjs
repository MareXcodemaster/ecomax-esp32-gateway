const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const source=fs.readFileSync(require.resolve('../src/panel_web/app.js'),'utf8');
const flow=source.slice(source.indexOf('async function run('),source.indexOf('async function saveTemperature('));
function context(route,states){
 const delays=[],calls=[];
 const ctx={state:{write_transport:route},actionBusy:false,actionEpoch:0,render:()=>{},notice:()=>{},noticeKind:'',csrf:'x',serverReachable:true,
  setTimeout:(fn,ms)=>{delays.push(ms);fn();},request:async(path)=>{calls.push(path);return path.startsWith('/api/job?')?states.shift():path==='/api/state'?ctx.state:{job:{id:'j'}};}};
 vm.runInNewContext(flow,ctx);return {ctx,delays,calls};
}
test('completed RS485 write is checked immediately without a fixed one-second pause',async()=>{
 const {ctx,delays,calls}=context('rs485',[{job:{id:'j',status:'done',result:'ok'}}]);
 assert.equal(await ctx.run('/api/write/execute',{token:'x'}),'ok');
 assert.deepEqual(delays,[]);assert.deepEqual(calls,['/api/write/execute','/api/job?id=j','/api/state']);
});
test('pending fast write polls after 250 ms and never repeats the command',async()=>{
 const {ctx,delays,calls}=context('rs485',[{job:{id:'j',status:'running'}},{job:{id:'j',status:'done',result:'ok'}}]);
 await ctx.run('/api/write/execute');assert.deepEqual(delays,[250]);
 assert.deepEqual(calls,['/api/write/execute','/api/job?id=j','/api/job?id=j','/api/state']);
});
test('running jobs never download the full catalogue before completion',async()=>{
 const states=Array.from({length:10},()=>({job:{id:'j',status:'running'}}));states.push({job:{id:'j',status:'done',result:'ok'}});
 const {ctx,calls}=context('rs485',states);await ctx.run('/api/write/execute');
 assert.equal(calls.filter(path=>path==='/api/state').length,1);assert.equal(calls.filter(path=>path.startsWith('/api/job?')).length,11);
});
test('cloud and boiler control job polling keep their prior interval',async()=>{
 for(const [route,path]of [['econet','/api/write/execute'],['rs485','/api/control/execute']]){
  const {ctx,delays}=context(route,[{job:{id:'j',status:'done'}}]);await ctx.run(path);assert.deepEqual(delays,[1000]);
 }
});
