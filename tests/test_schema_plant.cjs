const {test}=require('node:test');const assert=require('node:assert/strict');
const {sensors}=require('../src/panel_web/schema_plant.js');
test('graph includes measured temperatures, keeps zero and omits absent sensors',()=>{
 const rows=sensors({tempCO:65,tempCWU:45,tempExternalSensor:0,tempFeeder:27,tempFlueGas:112,tempBack:52,mixerTemp1:44,mixerTemp2:31,mixerSetTemp3:40,ecoSterTemp1:22.1,ecoSterTemp2:21.7,tempUpperBuffer:NaN,tempLowerBuffer:null});
 assert.equal(rows.length,10);assert.ok(rows.some(([,k])=>k==='tempExternalSensor'));assert.ok(!rows.some(([,k])=>k==='mixerTemp3'));assert.ok(!rows.some(([,k])=>k==='tempUpperBuffer'));
 assert.deepEqual(sensors({}),[]);
});
