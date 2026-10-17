const test=require('node:test');
const assert=require('node:assert/strict');
const {networks,scan}=require('../src/panel_web/wifi_scan.js');
const result=(sequence,busy=false)=>({sequence,busy,result:'ESP_OK',networks:[{ssid:' Example ',rssi:-55,authmode:3}]});

test('network list deduplicates by exact SSID, keeps strongest signal and filters invalid rows',()=>{
 const list=networks({networks:[{ssid:' Example ',rssi:-80,authmode:3},{ssid:' Example ',rssi:-55,authmode:3},
  {ssid:'Open',rssi:-40,authmode:0},{ssid:'<b>WiFi</b>',rssi:-60,authmode:3},
  {ssid:'',rssi:-30,authmode:0},{ssid:'x'.repeat(33),rssi:-30,authmode:3},{ssid:'bad\n',rssi:-30,authmode:3}]});
 assert.deepEqual(list.map(x=>x.ssid),['Open',' Example ','<b>WiFi</b>']);
 assert.equal(list[0].open,true);assert.equal(list[1].rssi,-55);
});
test('one scan request waits for a new sequence, not an old cached list',async()=>{
 const queue=[result(7),result(7),result(8,true),result(8)],calls=[];
 const items=await scan(async()=>queue.shift(),async()=>calls.push('start'),async()=>{});
 assert.deepEqual(calls,['start']);assert.equal(queue.length,0);assert.equal(items[0].ssid,' Example ');
});
test('failed or timed out scans never resend the scan command',async()=>{
 for(const fail of [false,true]){
  let reads=0,starts=0;
  await assert.rejects(scan(async()=>++reads===1?result(1):fail?{...result(2),result:'ESP_ERR_WIFI_STATE'}:result(1),async()=>starts++,async()=>{}));
  assert.equal(starts,1);assert.ok(reads<=51);
 }
});
test('empty scan succeeds and malformed firmware response is rejected before starting',async()=>{
 let reads=0;
 assert.deepEqual(await scan(async()=>++reads===1?result(0):{...result(1),networks:[]},async()=>{},async()=>{}),[]);
 let started=false;
 await assert.rejects(scan(async()=>({}),async()=>{started=true;},async()=>{}));
 assert.equal(started,false);
});
