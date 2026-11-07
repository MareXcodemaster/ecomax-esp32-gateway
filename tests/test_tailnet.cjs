const {test}=require('node:test');
const assert=require('node:assert/strict');
const {model,address}=require('../src/panel_web/tailnet.js');
test('Tailnet link requires fresh authenticated policy and valid assigned VPN address',()=>{
 const valid={connected:true,policy_ready:true,ip:'100.64.0.1'};
 assert.equal(address(valid),'http://100.64.0.1/');
 for(const ip of ['192.168.1.1','100.63.0.1','100.128.0.1','100.64.0.256','100.64.0.-1','100.64.0.1/evil','javascript:alert(1)','<img>'])assert.equal(address({...valid,ip}),'');
 for(const field of ['connected','policy_ready'])assert.equal(address({...valid,[field]:false}),'');
 assert.equal(address(null),'');
});
test('Tailnet displays stopped/failure states without inventing a connection or exposing keys',()=>{
 assert.equal(model({phase:'disabled'}).message,'Wyłączony');
 assert.match(model({phase:'memory_paused'}).message,/Panel lokalny nadal działa/);
 assert.match(model({phase:'paused_after_fault'}).message,/wstrzymany/);
 assert.equal(model({phase:'connected',connected:false,ip:'100.64.0.1'}).url,'');
 assert.match(model({key_saved:true,auth_key:'PRIVATE'}).key,/Klucz zapisany/);
 assert.ok(!JSON.stringify(model({auth_key:'PRIVATE'})).includes('PRIVATE'));
 assert.equal(model({internal_free_bytes:131072}).memory,'128 KiB wolnego RAM-u');
});
