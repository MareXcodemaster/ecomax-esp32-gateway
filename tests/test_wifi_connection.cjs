'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {validSsid,validPassword}=require('../src/panel_web/wifi_connection.js');
const {value}=require('../src/panel_web/heating_schema.js');
test('Wi-Fi names validate encoded length without trimming legitimate spaces',()=>{
 for(const v of ['',null,'a\n','a'.repeat(33),'\u0105'.repeat(17)])assert.equal(validSsid(v),false);
 for(const v of ['WiFi',' name with spaces ','\u0105'.repeat(16)])assert.equal(validSsid(v),true);
});
test('Wi-Fi credentials accept explicit open networks and WPA keys',()=>{
 assert.equal(validPassword('',true),true);assert.equal(validPassword('anything',true),false);
 for(const v of ['short','x'.repeat(64),'\u0105'.repeat(32),''])assert.equal(validPassword(v,false),false);
 for(const v of ['password', 'x'.repeat(63),'a'.repeat(64),'\u0105'.repeat(4)])assert.equal(validPassword(v,false),true);
});
test('schema does not render absent or nonfinite values as measurements',()=>{
 for(const v of [null,undefined,NaN,Infinity,'25'])assert.equal(value(v),'---');
 assert.equal(value(0),'0,0 \u00b0C');assert.equal(value(100,'%'),'100 %');
});
