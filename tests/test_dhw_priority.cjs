const {test}=require('node:test');
const assert=require('node:assert/strict');
const {dhwPriority}=require('../src/panel_web/mirror.js');
function fixture(){return {targets_fresh:true,targets:{'rm:8':{name:'Tryb pracy pompy CWU',before:1,min:0,max:2,options:[{value:0,label:'Wyłączony'},{value:1,label:'Priorytet'},{value:2,label:'Bez priorytetu'}]}}};}
test('priority toggle selects the controller mode, never switches DHW heating off',()=>{
 const data=fixture();assert.equal(dhwPriority(data,true).next,2);assert.equal(dhwPriority(data,true).writable,true);
 data.targets['rm:8'].before=2;assert.equal(dhwPriority(data,true).next,1);
 data.targets['rm:8'].before=0;assert.equal(dhwPriority(data,true).writable,false);assert.match(dhwPriority(data,true).label,/Grzanie CWU wyłączone/);
});
test('priority mode comes from unique labels, not fixed menu indices or enum values',()=>{
 const data=fixture(),t=data.targets['rm:8'];data.targets={'rm:91':t};t.before=4;t.min=0;t.max=7;t.options[1].value=4;t.options[2].value=7;
 assert.equal(dhwPriority(data,true).id,'rm:91');assert.equal(dhwPriority(data,true).next,7);
 data.targets['rm:92']={...t};assert.equal(dhwPriority(data,true).available,false);
});
test('priority control requires current readable writable supported mode',()=>{
 for(const mutate of [d=>d.targets_fresh=false,d=>d.targets['rm:8'].cached=true,d=>d.targets['rm:8'].write_ready=false,d=>d.targets['rm:8'].advanced=true,d=>d.targets['rm:8'].before=null,d=>d.targets['rm:8'].before='1',d=>d.targets['rm:8'].before=99,d=>d.targets['rm:8'].max=1]){const data=fixture();mutate(data);assert.equal(dhwPriority(data,true).writable,false);}
 assert.equal(dhwPriority(fixture(),false).writable,false);assert.equal(dhwPriority({targets:{}},true).available,false);
});
