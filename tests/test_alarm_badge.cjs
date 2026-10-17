const {test}=require('node:test');
const assert=require('node:assert/strict');
const {count}=require('../src/panel_web/alarms_badge.js');
const native=n=>({write_transport:'rs485',alarm_status:{pending_count:n,count_fresh:true,count_age_seconds:1},alarms:[{active:true},{active:false}]});
test('native badge counts current pending alerts, never the historical list',()=>{
 assert.equal(count(native(0),true),0);assert.equal(count(native(3),true),3);
 for(const pending of [null,-1,1.5,NaN,65536])assert.equal(count(native(pending),true),null);
 assert.equal(count(native(3),false),null);
 assert.equal(count({...native(3),alarm_status:{pending_count:3,count_fresh:false}},true),null);
 assert.equal(count({...native(3),alarm_status:{pending_count:3,count_fresh:true,count_age_seconds:16}},true),null);
});
test('ecoNET badge requires fresh system alarms and omits ended records',()=>{
 const data={write_transport:'econet',system_age_seconds:1,alarms:[1,{id:2,active:true},{id:3,active:false}]};
 assert.equal(count(data,true),2);assert.equal(count({...data,write_transport:'hybrid'},true),2);
 assert.equal(count({...data,system_age_seconds:46},true),null);assert.equal(count(data,false),null);
 assert.equal(count({...data,alarms:[]},true),0);assert.equal(count({...data,alarms:null},true),null);
});
