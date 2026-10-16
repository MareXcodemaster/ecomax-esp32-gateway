'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {scheduleHelp}=require('../src/panel_web/mirror.js');

test('all named schedules have specific descriptions without changing input',()=>{
 for(const key of ['boilerTZ','cwuTZ','circPumpTZ','boilerWorkTZ','boilerCleanTZ','exchangerCleanTZ','mixer1TZ','mixer2TZ','mixer3TZ','mixer4TZ','thermostat1TZ']){
  const help=scheduleHelp(key);for(const field of ['description','full','reduced','note'])assert.equal(typeof help[field],'string');
  assert(!help.description.includes('jeszcze opisane'));assert(help.full.startsWith('Turkusowe'));assert(help.reduced.startsWith('Szare'));
 }
});
test('CWU describes target reduction and not disabling the heater',()=>{
 const help=scheduleHelp('cwuTZ');assert(help.description.includes('kran'));assert(help.reduced.includes('nie jest ca\u0142kowite'));assert(help.note.includes('45\u00b0C'));assert(help.note.includes('40\u00b0C'));assert(help.note.includes('zmierzona'));
});
test('room schedule requires schedule mode and uses separate temperature profiles',()=>{
 const help=scheduleHelp('thermostat1TZ');assert(help.full.includes('komfortowa'));assert(help.reduced.includes('ekonomiczna'));assert(help.note.includes('Harmonogram'));assert(help.note.includes('Party'));
});
test('mixer descriptions identify the selected circuit and not the room temperature',()=>{
 for(let i=1;i<=4;i++){const help=scheduleHelp('mixer'+i+'TZ');assert(help.description.includes('mieszacza '+i+'.'));assert(help.description.includes('Nie jest')||help.description.includes('To nie jest'));}
 assert.notEqual(scheduleHelp('boilerTZ').description,scheduleHelp('boilerWorkTZ').description);
});
test('unknown and missing schedule types have a cautious fallback',()=>{
 for(const key of ['unknownTZ','embeddedThermo','mixer5TZ','',null,undefined]){const help=scheduleHelp(key);assert(help.description.includes('jeszcze opisane'));assert(help.full.includes('potwierdzenia'));}
});
test('plain descriptions explain what changes and the room mode requirement',()=>{
 assert(scheduleHelp('thermostat1TZ').description.startsWith('Prze\u0142\u0105cza temperatur\u0119 pokojow\u0105'));
 assert(scheduleHelp('thermostat1TZ').description.includes('Harmonogram'));
 assert(scheduleHelp('boilerWorkTZ').description.includes('nie oznacza ci\u0105g\u0142ego palenia'));
 assert(scheduleHelp('cwuTZ').description.includes('Nie zmienia temperatury w pokoju'));
});
test('examples distinguish their values from measurements and locate the settings',()=>{
 for(const key of ['cwuTZ','thermostat1TZ']){const note=scheduleHelp(key).note;assert(note.startsWith('Przyk\u0142ad:'));assert(note.includes('06:00'));assert(note.includes('Parametrach u\u017cytkownika'));}
 assert(scheduleHelp('thermostat1TZ').note.includes('Nocna nie jest automatycznie ni\u017csza'));
});
