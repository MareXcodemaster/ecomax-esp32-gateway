const test=require('node:test');
const assert=require('node:assert/strict');
const {tileRunning}=require('../src/panel_web/mirror.js');

test('tile motion reflects fresh power or an explicit fan output, never a missing reading',()=>{
 for(const key of ['boilerPower','fanPower']){
  for(const value of [1,50,100])assert.equal(tileRunning(key,{[key]:value},true),true);
  for(const value of [undefined,null,NaN,Infinity,-1,0,101,'50'])assert.equal(tileRunning(key,{[key]:value},true),false);
  assert.equal(tileRunning(key,{[key]:50},false),false);
 }
 assert.equal(tileRunning('fanPower',{fanWorks:true,fanPower:0},true),false);
 assert.equal(tileRunning('fanPower',{fanWorks:true},true),true);
 assert.equal(tileRunning('fanPower',{fanWorks:false,fanPower:50},true),false);
 assert.equal(tileRunning('fanPower',{fanWorks:true,fanPower:50},false),false);
 assert.equal(tileRunning('tempCO',{tempCO:75},true),false);
});

test('fan visualization speed increases proportionally with valid power',()=>{
 const {fanPeriod}=require('../src/panel_web/tile_motion.js');
 assert.equal(fanPeriod(100),.8);assert.equal(fanPeriod(50),1.6);assert.equal(fanPeriod(25),3.2);assert.equal(fanPeriod(10),8);
 for(const value of [0,-1,101,NaN,Infinity,'50'])assert.equal(fanPeriod(value),null);
 assert.equal(fanPeriod(null),2.4);
});
