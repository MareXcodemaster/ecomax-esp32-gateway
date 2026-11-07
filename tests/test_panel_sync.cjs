const test=require('node:test'),assert=require('node:assert/strict');
const {create}=require('../src/panel_web/sync.js');
const boot=()=>({sync:{schema:1,catalog:'0123456789abcdef',included:true},catalog:{names:{1:'CWU'},menu:[{index:0,name:'CWU',metadata:{minv:10,maxv:80}}],targets:{1:{name:'CWU',min:10,max:80}}},menu:[{index:0,metadata:{value:41},write_enabled:true}],targets:{1:{before:41}},fresh:true,curr:{tempCWU:40}});

test('unknown metadata stays unknown instead of inheriting cached limits',()=>{
 const s=create(),v=boot();v.menu[0].metadata=null;
 assert.equal(s.merge(v).menu[0].metadata,null);
});
test('live values from another controller client replace the old values without reloading names',()=>{
 const s=create(),first=s.merge(boot());assert.equal(first.menu[0].metadata.maxv,80);
 const delta=boot();delete delta.catalog;delta.sync.included=false;delta.targets[1].before=42;delta.menu[0].metadata.value=42;delta.curr.tempCWU=41;
 const next=s.merge(delta);assert.equal(next.targets[1].before,42);assert.equal(next.targets[1].max,80);assert.equal(next.menu[0].metadata.value,42);assert.equal(next.curr.tempCWU,41);
 assert.equal(first.targets[1].before,41);assert.equal(first.menu[0].metadata.value,41);
});
test('new boot replaces the catalogue and removes disappeared targets and permissions',()=>{
 const s=create();s.merge(boot());const changed=boot();changed.sync.catalog='1111111111111111';changed.catalog.targets={};changed.targets={};changed.menu[0].write_enabled=false;changed.catalog.menu[0].metadata.maxv=70;
 const next=s.merge(changed);assert.deepEqual(next.targets,{});assert.equal(next.menu[0].write_enabled,false);assert.equal(next.menu[0].metadata.maxv,70);
});
test('missing/mismatched catalogue is recovered with one fresh full compact request',async()=>{
 const s=create();s.merge(boot());const calls=[];
 const result=await s.load(async path=>{calls.push(path);if(calls.length===1){const d=boot();d.sync.included=false;d.sync.catalog='2222222222222222';delete d.catalog;return d;}return boot();});
 assert.equal(calls.length,2);assert.equal(calls[1],'/api/state?compact=1');assert.equal(result.targets[1].before,41);
});
test('legacy full response, failed polls, and stale readings never turn cached data fresh',async()=>{
 const s=create();s.merge(boot());await assert.rejects(s.load(async()=>{throw Error('offline');}));assert.equal(s.token,'0123456789abcdef');
 const stale=boot();stale.fresh=false;stale.targets={};stale.menu=[];const result=s.merge(stale);assert.equal(result.fresh,false);assert.deepEqual(result.targets,{});
 assert.deepEqual(s.merge({fresh:false,curr:{}}),{fresh:false,curr:{}});assert.equal(s.token,'');
});
