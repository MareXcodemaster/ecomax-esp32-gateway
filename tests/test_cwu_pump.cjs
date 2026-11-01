const test=require('node:test');
const assert=require('node:assert/strict');
const {paint,validWeek,validSettings}=require('../src/panel_web/cwu_pump.js');
const empty=()=>Array.from({length:7},()=>Array(6).fill(0));
test('CWU circulation week is Monday first and retains all 336 half-hours',()=>{
 const week=paint(empty(),[0,0],[0,0],true);assert.equal(week[0][0],128);assert.equal(week[6][0],0);
 const last=paint(week,[6,47],[6,47],true);assert.equal(last[6][5],1);assert.equal(last[0][0],128);
 const work=paint(empty(),[0,12],[4,15],true);
 assert.equal(work[0][1],15);assert.equal(work[4][1],15);assert.equal(work[5][1],0);
 assert.ok(validWeek(work));assert.deepEqual(paint(work,[0,12],[4,15],false),empty());
});
test('pump settings reject missing, reversed, nonfinite or too-close thresholds',()=>{
 const settings={lower:40,upper:45,mode:1,schedule_enabled:false,week:empty()};assert.ok(validSettings(settings));
 for(const patch of [{lower:NaN},{upper:Infinity},{lower:45},{upper:40.1},{lower:4},{upper:86},{mode:3},{schedule_enabled:'false'},{week:[]}])assert.equal(validSettings({...settings,...patch}),false);
 assert.throws(()=>paint(empty(),[7,0],[0,0],true));assert.throws(()=>paint(empty(),[0,0],[0,48],true));
});
