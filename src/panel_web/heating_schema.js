'use strict';
(function(root){
 const value=(v,unit='\u00b0C')=>typeof v==='number'&&Number.isFinite(v)?v.toLocaleString('pl-PL',{minimumFractionDigits:unit==='%'?0:1,maximumFractionDigits:1})+(unit?' '+unit:''):'---';
 if(typeof module==='object'&&module.exports)module.exports={value};
 if(typeof document==='undefined')return;
 let loading=null,svg=null,latest=null,retryAfter=0;
 function update(){
  if(!svg||!latest)return;
  const {data,fresh}=latest,c=fresh?data.curr||{}:{};
  const set=(id,text)=>{const node=svg.querySelector('#lbSchema_'+id);if(node)node.textContent=text;};
  set('model',data.system?.controllerID||'---');
  set('mode',fresh?EcoSettings.boilerModeLabel(c.mode,data.write_transport):'---');
  for(const [id,key]of [['tempExternalSensor','tempExternalSensor'],['ecoSterTemp1','ecoSterTemp1'],['tempFlueGas','tempFlueGas'],['tempCO','tempCO'],['tempCOSet','tempCOSet'],['tempBack','tempBack'],['tempCWU','tempCWU'],['tempCWUSet','tempCWUSet'],['mixerTempA','mixerTemp1'],['mixerTempASet','mixerSetTemp1']])set(id,value(c[key]));
  for(const [id,key,unit]of [['fuelLevel','fuelLevel','%'],['fuelStream','fuelStream','kg/h'],['fanPower','fanPower','%'],['tempOpticalSensor','tempOpticalSensor','']])set(id,value(c[key],unit));
  set('boilerPowerKW',typeof c.boilerPowerKW==='number'?value(c.boilerPowerKW,'kW'):value(c.boilerPower,'%'));
  set('mixerALabel','MIX1');
  for(const [id,key]of [['schema_pumpMixerA','mixerPumpWorks1'],['schema_pumpCWU','pumpCWUWorks']]){
   const node=svg.querySelector('#'+id);if(!node)continue;
   const active=c[key],color=typeof active==='boolean'?(active?'#28864a':'#333333'):'#999999';
   node.dataset.state=typeof active==='boolean'?(active?'on':'off'):'unknown';
   for(const path of node.querySelectorAll('path,circle,polygon')){
    if(path.getAttribute('stroke')&&path.getAttribute('stroke')!=='none')path.setAttribute('stroke',color);
    if(path.getAttribute('fill')&&path.getAttribute('fill')!=='none')path.setAttribute('fill',color);
   }
   node.setAttribute('aria-label',typeof active==='boolean'?(active?'ON':'OFF'):'Brak odczytu');
  }
  const container=document.getElementById('schema-readings');container.replaceChildren();
  for(const [name,key]of [['Kocio\u0142','tempCO'],['CWU','tempCWU'],['Mieszacz 1','mixerTemp1'],['Pomieszczenie','ecoSterTemp1']]){
   const box=document.createElement('div'),label=document.createElement('span'),reading=document.createElement('strong');
   label.textContent=name;reading.textContent=value(c[key]);box.append(label,reading);container.append(box);
  }
 }
 async function render(data,fresh){
  latest={data,fresh};const host=document.getElementById('schema-graphic'),readings=document.getElementById('schema-readings');
  host.setAttribute('role','group');
  if(Number(data.system?.schemaID)!==23){host.hidden=true;readings.textContent='Brak schematu dla tej konfiguracji';return;}
  host.hidden=false;
  if(svg){update();return;}
  if(loading||Date.now()<retryAfter)return;
  host.textContent='Wczytywanie schematu...';
  loading=(async()=>{
   try{
    const response=await fetch('/assets/schema.svg',{cache:'force-cache'});if(!response.ok)throw new Error('HTTP '+response.status);
    const parsed=new DOMParser().parseFromString(await response.text(),'image/svg+xml');
    if(parsed.querySelector('parsererror')||parsed.documentElement.localName!=='svg')throw new Error('Niepoprawna grafika');
    svg=document.importNode(parsed.documentElement,true);host.replaceChildren(svg);
    if(Number(latest.data.system?.schemaID)===23)update();
   }catch(error){retryAfter=Date.now()+30000;host.textContent='Nie uda\u0142o si\u0119 wczyta\u0107 schematu: '+error.message;}
   finally{loading=null;}
  })();
  await loading;
 }
 root.EcoHeatingSchema={render};
})(globalThis);
