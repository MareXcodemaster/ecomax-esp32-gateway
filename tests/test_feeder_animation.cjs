const test=require('node:test'),assert=require('node:assert/strict');
const {feederView}=require('../src/panel_web/pellet.js');
test('auger animation follows only a fresh boolean feeder output',()=>{
 assert.equal(feederView({fresh:true,feeder_on:true},true).state,'on');
 assert.equal(feederView({fresh:true,feeder_on:false},true).state,'off');
 for(const data of [null,{}, {fresh:true,feeder_on:null},{fresh:true,feeder_on:1},{fresh:true,feeder_on:'true'},{fresh:false,feeder_on:true}])assert.equal(feederView(data,true).state,'unknown');
 assert.equal(feederView({fresh:true,feeder_on:true},false).state,'unknown');
});
