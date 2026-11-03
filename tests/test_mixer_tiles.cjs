const {test}=require('node:test');const assert=require('node:assert/strict');
const {liveTiles,mixerTargetId,targetIdForTile}=require('../src/panel_web/mirror.js');
const state=()=>({curr:{mixerTemp1:44,mixerTemp2:31,mixerTemp3:null,mixerSetTemp3:40},menu:[{name:'Temperatura zadana mieszacza',category:'Ustawienia mieszacza 2',unit_name:'°C',write_enabled:true,write_id:'rm:91'}],targets:{'rm:91':{generic:true,value_kind:'number',unit:'°C',before:35,min:20,max:50,step:1}}});
test('live tiles add measured mixers even when the source catalogue contains one',()=>{
 const s=state();s.tiles=[{memberName_:'mixerTemp1',setMemberName_:'mixerSetTemp1'}];assert.deepEqual(liveTiles(s).map(t=>t.memberName_),['mixerTemp1','mixerTemp2']);assert.equal(liveTiles(s)[1].setMemberName_,'mixerSetTemp2');s.curr.mixerTemp2=0;assert.equal(liveTiles(s).length,2);
});
test('mixer tile resolves only its own single writable Celsius parameter',()=>{
 const s=state();assert.equal(targetIdForTile('mixerTemp2',s),'rm:91');assert.equal(mixerTargetId('mixerTemp1',s),null);
 s.menu[0].name='Histereza mieszacza';assert.equal(mixerTargetId('mixerTemp2',s),null);
 s.menu[0].name='Temperatura zadana mieszacza 2';assert.equal(mixerTargetId('mixerTemp2',s),'rm:91');
 s.menu[0].category='Ustawienia mieszacza 1';assert.equal(mixerTargetId('mixerTemp2',s),null);
 s.menu[0].category='Ustawienia mieszacza 2';s.menu.push({...s.menu[0]});assert.equal(mixerTargetId('mixerTemp2',s),null);s.menu.pop();
 s.targets['rm:91'].unit='%';assert.equal(mixerTargetId('mixerTemp2',s),null);s.targets['rm:91'].unit='°C';
 s.menu[0].write_enabled=false;assert.equal(mixerTargetId('mixerTemp2',s),null);s.menu[0].write_enabled=true;
 s.curr.mixerTemp2=null;assert.equal(mixerTargetId('mixerTemp2',s),null);
});
