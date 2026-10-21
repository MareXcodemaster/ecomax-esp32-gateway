const test=require('node:test'),assert=require('node:assert/strict');
const {levelView,readings}=require('../src/panel_web/pellet.js');
test('CAL replaces a percentage only when explicitly reported and fresh',()=>{
 assert.deepEqual(levelView(null,true,true),{percent:null,calibrating:true,label:'Kalibracja'});
 assert.equal(levelView(0,true,true).label,'Kalibracja');
 assert.equal(levelView(0,true,false).label,'0 %');
 assert.equal(levelView(100,true,false).label,'100 %');
 for(const flag of [undefined,null,false,'true',1])assert.equal(levelView(null,true,flag).label,'Brak odczytu');
 assert.equal(levelView(0,false,true).label,'Brak odczytu');
 assert.equal(readings({fresh:true,level_percent:0,level_calibrating:true},true).level,null);
});
