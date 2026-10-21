const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs');
const {calibrationPayload}=require('../src/panel_web/pellet.js');
test('pellet points require an explicit physical-level confirmation',()=>{
 for(const point of [0,100])assert.deepEqual(calibrationPayload(point,true),{point,confirm:true,physical_level_confirmed:true});
 for(const point of [null,undefined,50,'100',NaN])assert.throws(()=>calibrationPayload(point,true));
 assert.throws(()=>calibrationPayload(100,false));
});
test('boiler calibration dispatch is bounded, journaled and never retries or claims completed calibration',()=>{
 const cpp=fs.readFileSync('src/panel.cpp','utf8');
 const body=cpp.slice(cpp.indexOf('J *calibrate_pellet('),cpp.indexOf('void worker('));
 for(const guard of ['physical_level_confirmed','rs_identify()','pellet_model','rs_menu_catalog(true)','request_readonly(97','pellet_commands::configuration','journal()','pellet_commands::acknowledged'])assert.ok(body.includes(guard),guard);
 assert.equal(body.match(/transport::request\(112/g).length,1);
 assert.match(body,/flag\(result,"calibration_complete",false\)/);
 assert.doesNotMatch(body,/for\(|while\(|EEPROM_CL|request\(22/);
 const api=fs.readFileSync('src/api.cpp','utf8');assert.ok(api.includes('add("/api/pellet/calibrate",HTTP_POST,panel_command)'));
});
