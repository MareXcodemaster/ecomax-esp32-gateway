const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const h=require('../src/panel_web/history.js');
test('rolling history is limited to three days and uses real time rather than the last sample',()=>{
 const now=new Date('2026-10-14T22:00:00').getTime();
 const q=h.windowFor('4320','2026-10-14',now);assert.equal(q.to-q.from,3*86400);assert.equal(q.to,Math.floor(now/1000));
 assert.equal(h.windowFor('120','2026-10-14',now).to-h.windowFor('120','2026-10-14',now).from,7200);
 for(const value of ['0','10080','-1','foo'])assert.throws(()=>h.windowFor(value,'2026-10-14',now));
});
test('calendar selects a local day, clips retention and rejects future or malformed dates',()=>{
 const now=new Date('2026-10-14T22:00:00').getTime(),q=h.windowFor('day','2026-10-13',now);
 assert.equal(q.from,new Date('2026-10-13T00:00:00').getTime()/1000);assert.equal(q.to-q.from+1,86400);
 const oldest=h.windowFor('day','2026-10-11',now);assert.equal(oldest.from,Math.floor(now/1000)-3*86400);assert.equal(oldest.to,new Date('2026-10-12T00:00:00').getTime()/1000-1);
 for(const date of ['','2026-02-30','2026-10-15','2026-10-10','13.10.2026'])assert.throws(()=>h.windowFor('day',date,now));
});
test('local calendar follows daylight-saving day boundaries',()=>{
 const a=h.windowFor('day','2026-03-29',new Date('2026-03-30T12:00:00').getTime()),b=h.windowFor('day','2026-10-25',new Date('2026-10-26T12:00:00').getTime());
 const expectedA=(new Date('2026-03-30T00:00:00')-new Date('2026-03-29T00:00:00'))/1000,expectedB=(new Date('2026-10-26T00:00:00')-new Date('2026-10-25T00:00:00'))/1000;
 assert.equal(a.to-a.from+1,expectedA);assert.equal(b.to-b.from+1,expectedB);
});
test('decimated archive remains connected but explicit outages still split lines and state bands',()=>{
 const t=1700000000,p=(i,gapBefore=0)=>({time:t+i*360,co:40,native:1,gapBefore});
 assert.equal(h.segments([p(0),p(1),p(2)],'co',800).length,1);
 assert.equal(h.segments([p(0),p(1,1),p(2)],'co',800).length,2);
 assert.equal(h.intervals([p(0),p(1,1)],t+720,800)[0].end,t+60);
});
test('archive cannot overwrite app slots, automatically format unknown data or run on the RS485 worker',()=>{
 const read=name=>fs.readFileSync(path.join(__dirname,'../src',name),'utf8'),store=read('history_store.cpp'),data=read('history_data.hpp');
 assert(data.includes('retention_seconds=3*24*3600'));assert(store.includes('offset=0x620000,bytes=0x400000'));
 assert(store.includes('esp_partition_register_external'));assert(store.includes('conf.format_if_mount_failed=false'));assert(store.includes('if(!erased())'));
 assert(store.includes('fsync(fileno(file))'));assert(store.includes('remount_verified=probe()'));assert(store.includes('xQueueSend(queue,&sample,0)'));assert(store.includes('"history_flash",6144,nullptr,1'));
 assert(!/transport::|nvs_flash_erase|esp_restart/.test(store));
});
