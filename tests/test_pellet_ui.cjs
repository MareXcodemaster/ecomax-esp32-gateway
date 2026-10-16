const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const html=fs.readFileSync(require.resolve('../src/panel_web/index.html'),'utf8');
const source=fs.readFileSync(require.resolve('../src/panel_web/pellet.js'),'utf8');
test('pellet page removes the two requested unavailable tiles and their DOM writes',()=>{
 for(const id of ['pellet-flow','pellet-flow-source','pellet-consumed','pellet-storage']){
  assert.equal(html.includes(`id="${id}"`),false);
  assert.equal(source.includes(`get('${id}')`),false);
 }
 assert.ok(html.includes('id="pellet-level"'));
 assert.ok(html.includes('id="pellet-calibration-form"'));
});
test('pellet renderer works without removed nodes, online and offline',()=>{
 const nodes=new Map([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>[m[1],{value:'',valueAsNumber:NaN,textContent:'',disabled:false}]));
 const context={document:{getElementById:id=>{assert.ok(nodes.has(id),`Unexpected DOM access: ${id}`);return nodes.get(id);}}};
 vm.runInNewContext(source,context);
 const data={fresh:true,level_percent:50,calibration:{revision:1,sample_kg:null,sample_seconds:null,feeder_kg_h:null}};
 assert.doesNotThrow(()=>context.EcoPellet.render(data,true));
 assert.equal(nodes.get('pellet-level').textContent,'50 %');
 assert.doesNotThrow(()=>context.EcoPellet.render(null,false));
 assert.equal(nodes.get('pellet-level').textContent,'Brak odczytu');
});
