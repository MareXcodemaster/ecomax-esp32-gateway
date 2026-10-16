'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {econetNetworkValue,decodeSchedule}=require('../src/panel_web/mirror.js');
const {scheduleFresh,scheduleWritable,encodeSlots,reconcileSchedule}=require('../src/panel_web/settings.js');
const week=[[128,0,0,0,0,1],...Array.from({length:6},()=>[0,0,0,0,0,0]),[1,5,0,20]];
test('ecoNET network never falls back to emulated ESP fields',()=>{
 const data={values:{184:'ESP',181:100,182:1},econet_health:{fresh:true,api_ok:false},esp_network:{ssid:'Real ESP network'}};
 for(const id of ['172','181','182','184'])assert.equal(econetNetworkValue(id,data),'ecoNET niedost\u0119pny');
 assert.equal(econetNetworkValue('1025',data),undefined);
});
test('ecoNET network comes only from the separate HTTP snapshot',()=>{
 const data={values:{184:'ESP'},econet_network:{fresh:true,values:{184:'ecoNET network',181:72}}};
 assert.equal(econetNetworkValue('184',data),'ecoNET network');assert.equal(econetNetworkValue('181',data),72);
 assert.equal(econetNetworkValue('176',data),null);
 data.econet_network.fresh=false;assert.equal(econetNetworkValue('184',data),'Brak aktualnego odczytu ecoNET');
});
test('RS485 schedules use independent freshness, not cloud metadata freshness',()=>{
 const data={write_transport:'rs485',system_age_seconds:null,schedule_status:{fresh:true},schedule_write_indices:{cwuTZ:1},schedules:{ecomaxSchedules:{cwuTZ:week}}};
 assert.equal(scheduleFresh(data,true),true);assert.equal(scheduleWritable(data,'cwuTZ',true),true);
 data.schedule_status.fresh=false;assert.equal(scheduleWritable(data,'cwuTZ',true),false);
 data.schedule_status.fresh=true;assert.equal(scheduleWritable(data,'cwuTZ',false),false);
 delete data.schedule_write_indices.cwuTZ;assert.equal(scheduleWritable(data,'cwuTZ',true),false);
});
test('ecoNET schedule editing retains its own freshness guard',()=>{
 const data={write_transport:'econet',system_age_seconds:5,schedules:{ecomaxSchedules:{cwuTZ:week}}};
 assert.equal(scheduleWritable(data,'cwuTZ',true),true);data.system_age_seconds=46;assert.equal(scheduleWritable(data,'cwuTZ',true),false);
 data.system_age_seconds=null;assert.equal(scheduleWritable(data,'cwuTZ',true),false);
});
test('day bit order and weekly drafts remain exact',()=>{
 const slots=decodeSchedule(week,0).slots;assert.equal(slots[0],true);assert.equal(slots[47],true);assert.equal(slots[1],false);
 assert.deepEqual(encodeSlots(slots),week[0]);
 const draft={before:structuredClone(week),value:structuredClone(week)};draft.value[0][0]=0;
 assert.equal(reconcileSchedule(draft,week).changed,false);
 const external=structuredClone(week);external[2][1]=255;assert.equal(reconcileSchedule(draft,external).changed,true);
});
