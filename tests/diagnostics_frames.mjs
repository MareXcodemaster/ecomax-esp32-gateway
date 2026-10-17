// Synthetic, isolated diagnostics UI. No physical ESP and no boiler writes.
import assert from 'node:assert/strict';
import {readFile,mkdir} from 'node:fs/promises';
import {chromium} from 'playwright';

const source=await readFile('src/web/index.html','utf8');
const styles=[...source.matchAll(/<style>([\s\S]*?)<\/style>/g)].map(m=>m[0]).join('');
const section=source.match(/<section id="rs485"[\s\S]*?<\/section>/)[0].replace(' hidden','');
const script=source.match(/let lastFrameStats=0[\s\S]*?(?=\$\('export'\)\.onclick)/)[0];
const types=[10,106,107,137,185,192,234,235].map(type=>({type,count:12,name:'EXAMPLE_FRAME',label:'Ramka DEMO '+type,description:'Opis rozpoznanej zawartosci DEMO.',name_source:type===137?'econetanalyze_ecoster':'observed_bus_traffic',decode_state:[185,192].includes(type)?'decoded':'partial'}));
const thermostat={device_time:'2020-02-29 12:34:56',weekday_raw:6,model:'ecoSTER DEMO',target_temperature:20.5,room_temperature:19.75,unknown_fields:[{payload_offset:12,length:4,hex:'00010203'}]};
const fixture=type=>({records:[{ms:4800,source:type===137?81:69,destination:0,hex:'SYNTHETIC HEX',decoded:{state:[185,192].includes(type)?'decoded':'partial',source_name:type===137?'ecoSTER 1':'Sterownik kotla',destination_name:'Wszystkie urzadzenia',data:type===137?thermostat:type===192?{modules:[{address:69,device:'Sterownik kotla',software:'9.12.34',build_date:'Jan  1 2020 00:00:00',structure_version:0},{address:89,device:'ecoISM / modul radiowy',software:'1.2.3',structure_version:5}]}:type===185?{model:'ecoMAX DEMO',uid:'EXAMPLE123',product_type:0,product_id:42,logo_id:2,image_id:3}:{payload_bytes:0}}}]});
const browser=await chromium.launch({headless:true});await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  const page=await browser.newPage({viewport:{width,height:900}}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.setContent('<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width">'+styles+'<main class="wrap">'+section+'</main></html>');
  await page.evaluate(({types,fixtures})=>{
   window.$=id=>document.getElementById(id);window.text=(id,value)=>$(id).textContent=value;
   window.fmt=value=>Number(value).toLocaleString('pl-PL');window.lastStatus={uptime_ms:5000};
   window.calls=[];window.fixtures=fixtures;window.held=null;window.releaseHeld=null;
   window.request=async path=>{calls.push(path);if(path==='/api/frames')return {types};const type=Number(new URL(path,'http://example.invalid').searchParams.get('type'));if(held===type)await new Promise(resolve=>releaseHeld=resolve);return window.fixtures[type];};
  },{types,fixtures:Object.fromEntries(types.map(t=>[t.type,fixture(t.type)]))});
  await page.addScriptTag({content:script});
  await page.evaluate(async()=>{lastFrameStats=-1e9;await frameStats();});
  assert.equal(await page.locator('#frame-types tr').count(),8);
  assert.equal(await page.getByText('Cz\u0119\u015bciowo rozpoznane',{exact:true}).count(),6);
  assert.equal(await page.getByText('Nieopisany typ',{exact:true}).count(),0);
  await page.locator('[data-frame-type="137"]').click();
  await page.getByText('19,8 \u00b0C',{exact:true}).waitFor();
  assert.equal(await page.getByText('20,5 \u00b0C',{exact:true}).count(),1);
  assert.equal(await page.getByText('2020-02-29 12:34:56',{exact:true}).count(),1);
  assert.equal(await page.getByText('ecoSTER DEMO',{exact:true}).count(),1);
  assert.ok(await page.locator('#frame-detail').textContent().then(text=>text.includes('unknown_fields')));
  assert.equal(await page.locator('#frame-detail').isVisible(),false);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.screenshot({path:`test-results/diagnostics-thermostat-${width}.png`});
  await page.evaluate(async()=>{fixtures[137].records[0].decoded.data.room_temperature=18.5;await loadFrameDetails();});
  assert.equal(await page.getByText('18,5 \u00b0C',{exact:true}).count(),1);
  await page.locator('[data-frame-type="192"]').click();
  await page.getByText('9.12.34',{exact:true}).waitFor();
  assert.equal(await page.getByText('Nieprzes\u0142ana',{exact:true}).count(),1);
  await page.locator('[data-frame-type="185"]').click();await page.getByText('EXAMPLE123',{exact:true}).waitFor();
  await page.locator('[data-frame-type="235"]').click();
  assert.equal(await page.getByText('Temperatura zmierzona',{exact:true}).count(),0,'unknown radio values never become temperatures');
  await page.evaluate(()=>{renderFrameValues([{source:81,destination:0,ms:0,decoded:{state:'partial',data:{model:'<img src=x onerror=alert(1)>'}}}]);});
  assert.equal(await page.locator('#frame-decoded-values img').count(),0,'device strings remain text');
  await page.evaluate(()=>{held=137;});await page.locator('[data-frame-type="137"]').click();
  await page.locator('#close-frame-detail').click();await page.evaluate(()=>releaseHeld());
  assert.equal(await page.locator('#frame-inspector').isVisible(),false,'late response does not reopen closed inspector');
  const count=await page.evaluate(()=>calls.length);await page.evaluate(()=>loadFrameDetails());assert.equal(await page.evaluate(()=>calls.length),count);
  assert.ok((await page.evaluate(()=>calls)).every(path=>path==='/api/frames'||path.startsWith('/api/latest_frames?')));
  assert.deepEqual(errors,[]);await page.close();console.log(`PASS diagnostic fields ${width}px: labels, partial states, temperatures, modules, refresh, text safety, close race; reads only`);
 }
}finally{await browser.close();}
