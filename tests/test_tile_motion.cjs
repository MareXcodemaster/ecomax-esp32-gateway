const test=require('node:test');
const assert=require('node:assert/strict');
const {tileRunning}=require('../src/panel_web/mirror.js');

test('tile motion reflects fresh power or an explicit fan output, never a missing reading',()=>{
 for(const key of ['boilerPower','fanPower']){
  for(const value of [1,50,100])assert.equal(tileRunning(key,{[key]:value},true),true);
  for(const value of [undefined,null,NaN,Infinity,-1,0,101,'50'])assert.equal(tileRunning(key,{[key]:value},true),false);
  assert.equal(tileRunning(key,{[key]:50},false),false);
 }
 assert.equal(tileRunning('fanPower',{fanWorks:true,fanPower:0},true),true);
 assert.equal(tileRunning('fanPower',{fanWorks:false,fanPower:50},true),false);
 assert.equal(tileRunning('fanPower',{fanWorks:true,fanPower:50},false),false);
 assert.equal(tileRunning('tempCO',{tempCO:75},true),false);
});
