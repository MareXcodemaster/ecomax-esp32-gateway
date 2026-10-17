'use strict';
(function(root){
 const value=(v,unit='\u00b0C')=>typeof v==='number'&&Number.isFinite(v)?v.toLocaleString('pl-PL',{minimumFractionDigits:unit==='%'?0:1,maximumFractionDigits:1})+' '+unit:'---';
 function readings(data,fresh){
  const c=fresh?data.curr||{}:{};
  return {outside:value(c.tempExternalSensor),room:value(c.ecoSterTemp1),roomSet:value(c.ecoSterSetTemp1),
   co:value(c.tempCO),coSet:value(c.tempCOSet),water:value(c.tempCWU),waterSet:value(c.tempCWUSet),
   mixer:value(c.mixerTemp1),mixerSet:value(c.mixerSetTemp1),exhaust:value(c.tempFlueGas),back:value(c.tempBack),
   power:typeof c.boilerPowerKW==='number'?value(c.boilerPowerKW,'kW'):value(c.boilerPower,'%'),fuel:value(typeof c.fuelLevel==='number'&&c.fuelLevel>=0&&c.fuelLevel<=100?c.fuelLevel:null,'%'),fan:value(c.fanPower,'%'),
   mode:fresh?EcoSettings.boilerModeLabel(c.mode,data.write_transport):'Brak odczytu'};
 }
 if(typeof module==='object'&&module.exports)module.exports={value};
 if(typeof document==='undefined')return;
 let loading=null,svg=null,latest=null,retryAfter=0;
 function update(){
  if(!svg||!latest)return;
  const {data,fresh}=latest,c=fresh?data.curr||{}:{},values=readings(data,fresh);
  for(const node of svg.querySelectorAll('[data-value]'))node.textContent=values[node.dataset.value]||'---';
  for(const node of svg.querySelectorAll('[data-pump]')){const active=c[node.dataset.pump];node.dataset.state=typeof active==='boolean'?(active?'on':'off'):'unknown';node.querySelector('[data-pump-label]').textContent=typeof active==='boolean'?(active?'Praca':'Post\u00f3j'):'---';}
  document.getElementById('schema-readings').textContent=Number(data.system?.schemaID)===23?'Kocio\u0142, obieg mieszacza 1 i zasobnik CWU. Temperatury zadane to aktualne cele sterownika.':'Widok og\u00f3lny: kocio\u0142, mieszacz 1 i CWU. Nie potwierdza rzeczywistych po\u0142\u0105cze\u0144 ani wyposa\u017cenia tej instalacji.';
 }
 async function render(data,fresh){
  latest={data,fresh};const host=document.getElementById('schema-graphic');
  host.setAttribute('role','group');host.hidden=false;
  if(svg){update();return;}
  if(loading||Date.now()<retryAfter)return;
  host.textContent='Wczytywanie schematu...';
  loading=(async()=>{
   try{
    const response=await fetch('/assets/heating-schema.svg',{cache:'force-cache'});if(!response.ok)throw new Error('HTTP '+response.status);
    const parsed=new DOMParser().parseFromString(await response.text(),'image/svg+xml');
    if(parsed.querySelector('parsererror')||parsed.documentElement.localName!=='svg')throw new Error('Niepoprawna grafika');
    svg=document.importNode(parsed.documentElement,true);host.replaceChildren(svg);update();
   }catch(error){retryAfter=Date.now()+30000;host.textContent='Nie uda\u0142o si\u0119 wczyta\u0107 schematu: '+error.message;}
   finally{loading=null;}
  })();
  await loading;
 }
 root.EcoHeatingSchema={render};
})(globalThis);
