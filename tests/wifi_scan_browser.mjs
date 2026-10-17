import { chromium } from 'playwright';
import { readFile } from 'node:fs/promises';
import assert from 'node:assert/strict';

const html=await readFile('src/web/index.html','utf8');
const form=html.match(/<div class="band"><h2>Domowe Wi-Fi[\s\S]*?<\/form><\/div>/)[0];
const handlers=html.match(/let wifiScanning=false;[\s\S]*?(?=\$\('disconnect'\)\.onclick)/)[0];
const browser=await chromium.launch({headless:true});
try {
 for(const width of [1280,390]){
  const page=await browser.newPage({viewport:{width,height:900}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.setContent('<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><body>'+form+'</body></html>');
  await page.addStyleTag({content:html.match(/<style>([\s\S]*?)<\/style>/)[1]});
  await page.addScriptTag({content:await readFile('src/panel_web/wifi_scan.js','utf8')});
  await page.evaluate(()=>{
   window.$=id=>document.getElementById(id);window.text=(id,value)=>$(id).textContent=value;
   window.calls=[];let seq=0;
   window.request=async path=>{calls.push(['GET',path]);return {sequence:seq,busy:false,result:'ESP_OK',networks:[{ssid:'Moja siec',rssi:-48,authmode:3},{ssid:'<b>tekst</b>',rssi:-65,authmode:0}]};};
   window.jsonPost=async path=>{calls.push(['POST',path]);seq++;return {};};
   window.saveWifi=async (ssid,password)=>calls.push(['SAVE',ssid,password]);
  });
  await page.addScriptTag({content:handlers});
  await page.locator('#scan-wifi').click();
  await page.locator('#wifi-networks option[value="Moja siec"]').waitFor({state:'attached'});
  assert.equal(await page.evaluate(()=>calls.filter(c=>c[0]==='POST').length),1);
  assert.equal(await page.evaluate(()=>calls.filter(c=>c[0]==='SAVE').length),0);
  assert.equal(await page.locator('#wifi-networks b').count(),0);
  await page.locator('#wifi-networks').selectOption('Moja siec');
  assert.equal(await page.locator('#ssid').isVisible(),false);
  await page.locator('#wifi-password').fill('example-pass-123');
  await page.locator('[type=submit]').click();
  assert.deepEqual(await page.evaluate(()=>calls.filter(c=>c[0]==='SAVE')),[['SAVE','Moja siec','example-pass-123']]);
  await page.locator('#wifi-networks').selectOption({label:'Siec ukryta / wpisz nazwe recznie'});
  assert.equal(await page.locator('#ssid').isVisible(),true);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  assert.deepEqual(errors,[]);
  await page.close();
  console.log(`PASS Wi-Fi scan phone configuration ${width}px: one scan, no implicit configuration write, text-only SSIDs and hidden-network fallback`);
 }
}finally{await browser.close();}
