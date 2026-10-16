const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const h=require('../src/panel_web/history.js'),{boilerModeLabel}=require('../src/panel_web/settings.js');
const t=1700000000,p=(time,co,native=1)=>({time,co,native,mode:2});
test('history decodes the compact matrix and retains nulls and true zero values',()=>{
 const data={schema:1,columns:['time','native','mode','co','fuel','flow'],rows:[[t,1,2,0,0,null]]};
 assert.deepEqual(h.decode(data),[{time:t,native:1,mode:2,co:0,fuel:0,flow:null}]);
 for(const bad of [{...data,schema:2},{...data,columns:['time','time','mode']},{...data,rows:[[t,1]]},{...data,rows:[[0,1,2,0,0,0]]},{...data,rows:[[t,4,2,0,0,0]]},{...data,rows:[[t,1,2,0,0,0],[t-1,1,2,0,0,0]]}])assert.throws(()=>h.decode(bad));
});
test('history keeps the most recent 720 records only',()=>{
 const rows=Array.from({length:750},(_,i)=>[t+i*10,1,3]);assert.equal(h.decode({schema:1,columns:['time','native','mode'],rows}).length,720);
});
test('graphs break at missing values, source switches and communication outages',()=>{
 const data=[p(t,0),p(t+10,1),p(t+20,null),p(t+30,3),p(t+80,4),p(t+90,5,0)];
 assert.deepEqual(h.segments(data,'co').map(part=>part.map(p=>p.co)),[[0,1],[3],[4],[5]]);
 assert.deepEqual(h.intervals(data,t+100).map(i=>i.end-i.start),[10,10,10,30,10,10]);
});
test('native and ecoNET history use their own phase mapping including stabilization aliases',()=>{
 assert.equal(h.phase({mode:2,native:1},boilerModeLabel),'Rozpalanie');
 assert.equal(h.phase({mode:12,native:1},boilerModeLabel),'Stabilizacja');
 assert.equal(h.phase({mode:23,native:1},boilerModeLabel),'Stabilizacja');
 assert.equal(h.phase({mode:6,native:1},boilerModeLabel),'Post\u00f3j');
 assert.equal(h.phase({mode:6,native:0},boilerModeLabel),'Czyszczenie');
 assert.equal(h.phase({mode:null,native:1},boilerModeLabel),'Brak odczytu');
 assert.equal(h.phase({mode:99,native:1},boilerModeLabel),'Stan 99');
});
test('CSV exports all measurements and outputs, never replacing absent measurements with zero',()=>{
 const csv=h.csv([{...p(t,0),fuel:0,flow:null,pumpCO:0,lighter:1}],boilerModeLabel);
 assert(csv.startsWith('\uFEFF'));assert(csv.includes('Rozpalanie'));assert(csv.includes('Strumie\u0144 pelletu [kg/h]'));assert(csv.includes('Zapalarka'));assert(csv.includes('"0"'));assert(csv.includes('""'));
});
test('history collection is bounded in PSRAM with no extra bus requests or flash writes',()=>{
 const source=fs.readFileSync(path.join(__dirname,'../src/panel_history.cpp'),'utf8');
 assert(source.includes('capacity=720'));assert(source.includes('MALLOC_CAP_SPIRAM'));assert(source.includes('monotonic_ms-last_ms>=interval_ms||transition'));
 assert(!/nvs_set|transport::|esp_restart|uart_write/.test(source));
 const api=fs.readFileSync(path.join(__dirname,'../src/api.cpp'),'utf8');assert(api.includes('add("/api/history",HTTP_GET,history_state)'));
});
