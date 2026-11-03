const {test}=require('node:test');const assert=require('node:assert/strict');
const {circuits,value}=require('../src/panel_web/heating_schema.js');
test('schema follows observed circuits, not the reference boiler schema ID',()=>{
 const data={system:{controllerID:'ecoMAX810P-L TOUCH'},curr:{tempCWU:45,mixerTemp1:31,mixerTemp2:28,mixerSetTemp3:40,mixerTemp3:null,ecoSterTemp1:22}};
 assert.deepEqual(circuits(data).map(r=>r.key),['tempCWU','mixerTemp1','mixerTemp2']);
 assert.equal(circuits({...data,system:{schemaID:99}}).length,3);
 assert.deepEqual(circuits({curr:{mixerTemp1:'31',tempCWU:null,tempUpperBuffer:50}}).map(r=>r.key),['tempUpperBuffer']);
 assert.deepEqual(circuits({}),[]);assert.equal(value(null),'---');
});
