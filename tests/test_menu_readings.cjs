const test=require('node:test');
const assert=require('node:assert/strict');
const {parameterReading}=require('../src/panel_web/mirror.js');
const fresh={targets_fresh:true};
test('read-only service parameters display their actual numeric value, including zero',()=>{
 for(const value of [0,1,0.9,45,100])assert.equal(parameterReading({metadata:{value},read_available:true,write_enabled:false},fresh,true),value);
});
test('failed or expired reads do not display cached values or imply OFF',()=>{
 const row={metadata:{value:0},read_available:false,read_status:'Brak odczytu RS485'};
 assert.equal(parameterReading(row,fresh,true),'Brak odczytu RS485');
 assert.equal(parameterReading({...row,read_available:true},fresh,false),'Brak aktualnego odczytu');
 assert.equal(parameterReading({...row,read_available:true},{targets_fresh:false},true),'Brak aktualnego odczytu');
});
test('unsupported and legacy values are distinguished from a missing connection',()=>{
 assert.equal(parameterReading({metadata:{value:null}},fresh,true),'Nieudost\u0119pniony');
 assert.equal(parameterReading({metadata:{value:42}},fresh,true),42);
});
