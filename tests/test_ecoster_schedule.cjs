const test=require('node:test'),assert=require('node:assert/strict');
const {scheduleFresh,scheduleWritable,validSchedule,roomMode,roomModes}=require('../src/panel_web/settings.js');
const week=()=>[...Array.from({length:7},()=>[0,0,0,0,0,0]),[1,0,0,0]];
test('room schedules require their own fresh reply, not a fresh boiler schedule',()=>{
 const data={write_transport:'rs485',schedule_status:{fresh:true},thermostat_schedule_status:{fresh:false},schedules:{ecomaxSchedules:{thermostat1TZ:week(),cwuTZ:week()}},schedule_write_indices:{thermostat1TZ:16,cwuTZ:1}};
 assert.equal(scheduleWritable(data,'thermostat1TZ',true),false);assert.equal(scheduleWritable(data,'cwuTZ',true),true);
 data.schedule_status.fresh=false;data.thermostat_schedule_status.fresh=true;
 assert.equal(scheduleWritable(data,'thermostat1TZ',true),true);assert.equal(scheduleWritable(data,'cwuTZ',true),false);
 assert.equal(scheduleFresh(data,false,'thermostat1TZ'),false);
});
test('a valid room week without verified write mapping remains read-only',()=>{
 const data={write_transport:'rs485',thermostat_schedule_status:{fresh:true},schedules:{ecomaxSchedules:{thermostat1TZ:week()}},schedule_write_indices:{}};
 assert.equal(validSchedule(week()),true);assert.equal(scheduleWritable(data,'thermostat1TZ',true),false);
});
test('room labels distinguish schedule and manual modes and preserve extra flag decoding',()=>{
 assert.equal(roomModes[roomMode(0)],'Harmonogram');assert.equal(roomModes[roomMode(130)],'Komfortowy');assert.equal(roomMode(undefined),null);
});
