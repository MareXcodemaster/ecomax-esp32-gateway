const test=require('node:test');
const assert=require('node:assert/strict');
const {decodeSchedule,buildTree,visibleTree,reconcileDraft,clock,targetIdForTile,writeIdForEntry,ticketMatches}=require('../src/panel_web/mirror.js');
test('MSB-first CWU schedule, exact half-hour ranges and Sunday first',()=>{
 const raw=Array.from({length:7},()=>[128,31,128,0,31,255]);raw.push([1,5,0,20]);
 const d=decodeSchedule(raw,0);
 assert.equal(d.enabled,true);assert.equal(d.reduction,5);assert.equal(d.slots.length,48);
 assert.deepEqual(d.ranges.map(r=>[r.full,r.start,r.end]),[[true,0,1],[false,1,11],[true,11,17],[false,17,35],[true,35,48]]);
 assert.equal(clock(48),'24:00');
});
test('malformed schedules never look like an active timetable',()=>{
 for(const raw of [null,[],[[256]],Array(8).fill([0])])assert.equal(decodeSchedule(raw,0),null);
 const raw=Array.from({length:7},()=>[0,0,0,0,0,0]);raw.push([0,0,0,20]);
 assert.equal(decodeSchedule(raw,7),null);assert.equal(decodeSchedule(raw,0).enabled,false);
 assert.deepEqual(decodeSchedule(raw,0).ranges.map(r=>[r.full,r.start,r.end]),[[false,0,48]]);
});
test('menu cursor preserves sections, ordering and current data ID',()=>{
 const s=[{type:7},{type:0,index:0},{type:7},{type:0,index:1},{type:5,index:2},{type:7},{type:1,index:47},{type:3,index:9,data_id:1024},{type:7},{type:0,index:3,pass_index:1},{type:0,index:4,pass_index:2},{type:7},{type:1,index:80,pass_index:1},{type:7},{type:1,index:90,pass_index:2}];
 const tree=buildTree(s,['Menu','CWU','Ustawienia serwisowe','Rozpalanie','Producent']);
 const user=visibleTree(tree,false),service=visibleTree(tree,true);
 assert.equal(user.entries.length,1);assert.deepEqual(user.entries[0].entries.map(e=>e.index),[47,9]);
 assert.equal(user.entries[0].entries[1].data_id,1024);
 assert.equal(service.entries.length,1);assert.equal(service.entries[0].name,'Rozpalanie');assert.equal(service.entries[0].entries[0].index,80);
});
test('duplicate information sections keep parent ordinal aliases',()=>{
 const tree=buildTree([{type:7},{type:0,index:0},{type:7},{type:2,index:1},{type:7},{type:2,index:2},{type:2,index:3},{type:7},{type:3,data_id:100},{type:7},{type:3,data_id:101}],['Menu','Informacje','Informacje','Informacje']);
 assert.equal(tree.entries.length,1);assert.deepEqual(tree.entries[0].entries.map(e=>e.data_id),[100,101]);
});
test('untouched temperature input follows external setting changes',()=>{
 assert.deepEqual(reconcileDraft({value:'40',baseline:40,dirty:false},42,true),{value:'42',baseline:42,dirty:false,changed:false});
});
test('external changes refresh current setting without overwriting a user draft',()=>{
 assert.deepEqual(reconcileDraft({value:'45',baseline:40,dirty:true},42,true),{value:'45',baseline:40,dirty:true,changed:true});
});
test('expired readings clear old defaults but preserve explicitly typed values',()=>{
 assert.equal(reconcileDraft({value:'40',baseline:40,dirty:false},42,false).value,'');
 assert.equal(reconcileDraft({value:'45',baseline:40,dirty:true},42,false).value,'45');
});
test('input resumes live defaults after stale connection recovers',()=>{
 const stale=reconcileDraft({value:'40',baseline:40,dirty:false},undefined,false);
 assert.equal(reconcileDraft(stale,43,true).value,'43');
});
test('mixer and room tiles route to actual setpoints, never measured temperatures',()=>{
 assert.equal(targetIdForTile('mixerTemp1',{}),'1287');
 assert.equal(targetIdForTile('ecoSterTemp1',{curr:{ecoSterMode1:2}}),'STER_TEMP_DAY_1');
 assert.equal(targetIdForTile('ecoSterTemp1',{curr:{ecoSterMode1:1}}),'STER_TEMP_NIGHT_1');
 assert.equal(targetIdForTile('ecoSterTemp1',{curr:{ecoSterMode1:5}}),'legacy:STER_TEMP_SET_PARTY_1');
 assert.equal(targetIdForTile('mixerTemp2',{}),null);
});
test('menu editing needs an explicit enabled write ID',()=>{
 assert.equal(writeIdForEntry({write_enabled:true,write_id:'1287'}),'1287');
 assert.equal(writeIdForEntry({write_enabled:false,write_id:'1287'}),null);
 assert.equal(writeIdForEntry({metadata:{edit:true}}),null);
});
test('confirmation tolerates float readback but rejects changed value, mode, route and stale data',()=>{
 const ticket={id:'STER_TEMP_DAY_1',before:22.7,min:10,max:35,step:0.1,legacy:true,profile_mode:2,transport:'econet',expires:100};
 const live={...ticket,before:22.69999885559082},data={targets:{[ticket.id]:live},targets_fresh:true,write_transport:'econet'};
 assert.equal(ticketMatches(data,ticket,true,90000),true);
 for(const [key,value]of [['before',22.8],['min',11],['max',34],['step',1],['profile_mode',3]])assert.equal(ticketMatches({...data,targets:{[ticket.id]:{...live,[key]:value}}},ticket,true,90000),false);
 assert.equal(ticketMatches({...data,write_transport:'rs485'},ticket,true,90000),false);
 assert.equal(ticketMatches({...data,targets_fresh:false},ticket,true,90000),false);
 assert.equal(ticketMatches(data,ticket,false,90000),false);
 assert.equal(ticketMatches(data,ticket,true,100000),false);
});
