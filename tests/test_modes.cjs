const test=require('node:test');
const assert=require('node:assert/strict');
const {boilerModeLabel}=require('../src/panel_web/settings.js');
test('native and hybrid use the SENSOR_DATA state table, including stabilization and standby',()=>{
 for(const route of ['rs485','hybrid']){
  assert.equal(boilerModeLabel(1,route),'Stabilizacja');assert.equal(boilerModeLabel(6,route),'Post\u00f3j');assert.equal(boilerModeLabel(7,route),'Wygaszanie');
 }
});
test('ecoNET retains its own state labels and unknown codes remain visible',()=>{
 assert.equal(boilerModeLabel(6,'econet'),'Czyszczenie');assert.equal(boilerModeLabel(251,'rs485'),'Stan 251');
});
test('extended RS485 stabilization codes are named without changing cloud states',()=>{
 for(const route of ['rs485','hybrid'])for(const code of [12,23])assert.equal(boilerModeLabel(code,route),'Stabilizacja');
 assert.equal(boilerModeLabel(23,'econet'),'Rozgrzewanie');
});
