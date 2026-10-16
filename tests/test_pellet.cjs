const test=require('node:test');
const assert=require('node:assert/strict');
const {calibrationRate,readings}=require('../src/panel_web/pellet.js');
test('weighed feeder calibration uses actual running seconds',()=>{
 assert.equal(calibrationRate(1,360),10);assert.equal(calibrationRate(0.15,60),9);
});
test('unknown, nonfinite and out-of-range calibration stays unavailable',()=>{
 for(const [kg,seconds] of [[null,60],[1,null],[NaN,60],[1,Infinity],[0,60],[-1,60],[1,0],[25,10],[1,3601],[26,3600]])assert.equal(calibrationRate(kg,seconds),null);
});
test('zero fuel/flow is distinct from a missing reading',()=>{
 assert.deepEqual(readings({fresh:true,level_percent:0,flow_kg_h:0,reported_flow_kg_h:null,consumed_kg:0,flow_source:'feeder_estimate'},true),{level:0,flow:0,reported:null,consumed:0,source:'feeder_estimate'});
 assert.deepEqual(readings({fresh:true,level_percent:null,flow_kg_h:null,consumed_kg:null,flow_source:'unavailable'},true),{level:null,flow:null,reported:null,consumed:null,source:'unavailable'});
});
test('stale sensor cannot look live; saved counter is separate',()=>{
 const data={fresh:false,level_percent:75,flow_kg_h:2,reported_flow_kg_h:2,consumed_kg:8,flow_source:'boiler'};
 assert.deepEqual(readings(data,true),{level:null,flow:null,reported:null,consumed:8,source:'unavailable'});
 assert.deepEqual(readings(data,false),{level:null,flow:null,reported:null,consumed:null,source:'unavailable'});
 assert.equal(readings(null,true).consumed,null);
});
