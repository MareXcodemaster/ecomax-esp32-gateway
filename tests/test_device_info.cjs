const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs');
const {bytes,duration,reset,usage,model}=require('../src/panel_web/device_info.js');
const fixture=require('./fixtures/device_info.json');

test('memory percentages use the correct pool, not physical flash or combined free heap',()=>{
 const data=model(fixture,true);
 assert.equal(data.pools.firmware.total_bytes,3145728);
 assert.ok(Math.abs(data.pools.firmware.percent-53.333346)<.0001);
 assert.equal(data.pools.ram.percent,78.125);assert.equal(data.pools.psram.percent,37.5);assert.equal(data.pools.history.percent,18.75);
 assert.equal(data.details.find(([name])=>name==='Flash urzadzenia')[1],'16 MiB');
 assert.equal(data.details.find(([name])=>name==='Przyczyna ostatniego restartu')[1],'Restart programowy');
});
test('missing, inconsistent or invalid memory never looks empty or fully free',()=>{
 for(const value of [null,{}, {total_bytes:0,used_bytes:0,free_bytes:0},{total_bytes:100,used_bytes:101,free_bytes:0},{total_bytes:100,used_bytes:1,free_bytes:100},{total_bytes:100,used_bytes:-1,free_bytes:101},{total_bytes:100,used_bytes:NaN,free_bytes:0}])assert.equal(usage(value),null);
 assert.equal(usage({total_bytes:100,used_bytes:0,free_bytes:100}).percent,0);
 assert.equal(usage({total_bytes:100,used_bytes:100,free_bytes:0}).percent,100);
 assert.equal(model(null,true),null);assert.equal(model(fixture,false),null);assert.equal(model({...fixture,schema:2},true),null);
 const data=model({...fixture,history:{...fixture.history,ready:false,error:'Test'},psram:{total_bytes:0,used_bytes:0,free_bytes:0}},true);
 assert.equal(data.pools.history,null);assert.equal(data.pools.psram,null);assert.match(data.history_error,/Test/);
});
test('byte sizes, uptime and reset labels preserve zero and unknown values',()=>{
 assert.equal(bytes(0),'0 B');assert.equal(bytes(1024),'1 KiB');assert.equal(bytes(8388608),'8 MiB');
 for(const value of [null,undefined,NaN,-1,'1024'])assert.equal(bytes(value),'---');
 assert.equal(duration(0),'00:00:00');assert.equal(duration(93784000),'1 d 02:03:04');assert.equal(duration(59000),'00:00:59');
 assert.equal(duration(null),'---');assert.equal(reset(0),'Nieznana');assert.equal(reset(99),'Kod 99');assert.equal(reset(null),'---');
});
test('ESP data remains available when the boiler is offline and performs no new polling',()=>{
 assert.ok(model({...fixture,fresh:false},true));
 const cpp=fs.readFileSync('src/device_info.cpp','utf8'),panel=fs.readFileSync('src/panel.cpp','utf8'),js=fs.readFileSync('src/panel_web/device_info.js','utf8'),app=fs.readFileSync('src/panel_web/app.js','utf8');
 assert.match(cpp,/static const Hardware hardware=hardware_info\(\)/);
 assert.match(cpp,/esp_image_get_metadata/);assert.doesNotMatch(cpp,/esp_image_verify\(|esp_flash_read\(|transport::|request_readonly|esp_restart|nvs_set/);
 assert.match(panel,/put\(out,"esp_device",device_info::snapshot\(\)\)/);
 assert.match(app,/EcoDevice.render\(state.esp_device,serverReachable\)/);
 assert.doesNotMatch(js,/fetch\(|request\(|setInterval\(|innerHTML/);
});
