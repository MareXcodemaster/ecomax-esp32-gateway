const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const source=fs.readFileSync(require.resolve('../src/panel_web/app.js'),'utf8');
const flow=source.slice(source.indexOf('function preflightReadFailure('),source.indexOf('async function request('));
function context(){
 const element={hidden:true,textContent:'',classList:{toggle:()=>{}}};
 const ctx={noticeKind:'',noticeError:false,lastNoticeJob:null,serverReachable:true,
  state:{fresh:true,targets_fresh:true,error:null,menu_read_error:'',target_read_ms:50,rs485_identity_verified_ms:50,
   job:{id:'j',kind:'rs_probe',status:'failed',finished_ms:100,error:'Brak identyfikacji kotla po RS485'}},
  text:(_,value)=>{element.textContent=value;},$:()=>element};
 vm.runInNewContext(flow,ctx);return {ctx,element};
}
test('failed identification remains visible until identity is verified after that failure',()=>{
 const {ctx,element}=context();ctx.renderJobNotice();assert.equal(element.hidden,false);
 ctx.state.target_read_ms=110;ctx.renderJobNotice();assert.equal(element.hidden,false);
 ctx.state.rs485_identity_verified_ms=120;ctx.renderJobNotice();assert.equal(element.hidden,true);
});
test('opening the panel after read recovery does not resurrect a diagnostic error',()=>{
 const {ctx,element}=context();ctx.state.rs485_identity_verified_ms=120;
 ctx.renderJobNotice();assert.equal(element.hidden,true);assert.equal(ctx.lastNoticeJob,'j');
});
test('a new sensor sample alone does not clear a failed catalogue refresh',()=>{
 const {ctx,element}=context();ctx.state.job.kind='refresh';ctx.state.job.error='Brak odczytu nastaw RS485';
 ctx.renderJobNotice();assert.equal(element.hidden,false);
 ctx.state.target_read_ms=110;ctx.state.menu_read_error='Niezgodny katalog';ctx.renderJobNotice();assert.equal(element.hidden,false);
 ctx.state.menu_read_error='';ctx.renderJobNotice();assert.equal(element.hidden,true);
});
test('offline, stale and current errors do not hide a failed diagnostic',()=>{
 for(const change of [c=>{c.serverReachable=false;},c=>{c.state.fresh=false;},c=>{c.state.targets_fresh=false;},c=>{c.state.error='Brak identyfikacji';}]){
  const {ctx,element}=context();ctx.state.rs485_identity_verified_ms=120;change(ctx);ctx.renderJobNotice();assert.equal(element.hidden,false);
 }
});
test('read recovery never dismisses a failed or unverified write warning',()=>{
 const {ctx,element}=context();ctx.state.job.kind='execute';ctx.renderJobNotice();
 ctx.state.rs485_identity_verified_ms=120;ctx.state.target_read_ms=130;ctx.renderJobNotice();assert.equal(element.hidden,false);
 ctx.notice('Zmiana niepotwierdzona',true);ctx.notice('Panel niedostepny',true,'connection');
 assert.equal(element.textContent,'Zmiana niepotwierdzona');assert.equal(ctx.noticeKind,'operation');
});
test('another reader diagnostic cannot replace an unverified write warning',()=>{
 const {ctx,element}=context();ctx.notice('Zmiana niepotwierdzona',true);ctx.renderJobNotice();
 assert.equal(element.textContent,'Zmiana niepotwierdzona');assert.equal(ctx.noticeKind,'operation');
 ctx.state.rs485_identity_verified_ms=120;ctx.renderJobNotice();assert.equal(element.hidden,false);
});

test('old read-only preparation failure is not resurrected after a complete read recovery',()=>{
 const {ctx,element}=context();Object.assign(ctx.state,{write_transport:'rs485',target_read_ms:120,targets:{STER_TEMP_DAY_1:{before:21.9}},target_errors:{},job:{id:'old',kind:'prepare',status:'failed',finished_ms:100,error:'Brak zgodnego odczytu nastawy RS485'}});
 ctx.renderJobNotice();assert.equal(element.hidden,true);
});

test('preflight failure recovers only after the selected parameter is readable again',()=>{
 const {ctx,element}=context();Object.assign(ctx.state,{write_transport:'rs485',targets:{},target_errors:{},job:{id:'new',kind:'save',dispatch_started:false,setting_id:'STER_TEMP_DAY_1',status:'failed',finished_ms:100,error:'Brak zgodnego odczytu nastawy RS485'}});
 ctx.renderJobNotice();assert.equal(ctx.noticeKind,'preflight');assert.equal(element.hidden,false);
 ctx.state.target_read_ms=120;ctx.renderJobNotice();assert.equal(ctx.noticeError,true);
 ctx.state.targets.STER_TEMP_DAY_1={write_ready:false};ctx.renderJobNotice();assert.equal(ctx.noticeError,true);
 ctx.state.targets.STER_TEMP_DAY_1={write_ready:true};ctx.renderJobNotice();assert.equal(ctx.noticeError,false);assert.match(element.textContent,/Poprzedniej zmiany nie wyslano/);
});

test('recovery cannot hide a dispatched, unknown, unsupported or offline command outcome',()=>{
 for(const change of [c=>{c.state.job.dispatch_started=true;},c=>{delete c.state.job.dispatch_started;},c=>{c.state.job.kind='control_execute';},c=>{c.state.job.error='Niezweryfikowany model';},c=>{c.serverReachable=false;},c=>{c.state.fresh=false;},c=>{c.state.target_read_ms=90;},c=>{delete c.state.target_read_ms;}]){
  const {ctx,element}=context();Object.assign(ctx.state,{write_transport:'rs485',target_read_ms:120,targets:{x:{before:22}},job:{id:'write',kind:'save',dispatch_started:false,setting_id:'x',status:'failed',finished_ms:100,error:'Brak zgodnego odczytu nastawy RS485'}});change(ctx);
  ctx.renderJobNotice();assert.equal(element.hidden,false);assert.equal(ctx.noticeError,true);
 }
});
