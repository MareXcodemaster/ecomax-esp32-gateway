'use strict';
(function(root){
 const value=(v,unit='\u00b0C')=>typeof v==='number'&&Number.isFinite(v)?v.toLocaleString('pl-PL',{minimumFractionDigits:unit==='%'?0:1,maximumFractionDigits:1})+(unit?' '+unit:''):'---';
 const finite=v=>typeof v==='number'&&Number.isFinite(v);
 function circuits(data){
  const c=data.curr||{},rows=[];
  if(finite(c.tempCWU))rows.push({name:'Zasobnik CWU',key:'tempCWU',target:'tempCWUSet',pump:'pumpCWUWorks',kind:'tank'});
  for(let i=1;i<=16;i++)if(finite(c['mixerTemp'+i]))rows.push({name:'Mieszacz '+i,key:'mixerTemp'+i,target:'mixerSetTemp'+i,pump:'mixerPumpWorks'+i,kind:'mixer'});
  for(const [name,key]of [['Bufor — góra','tempUpperBuffer'],['Bufor — dół','tempLowerBuffer'],['Sprzęgło hydrauliczne','tempHydraulicCoupler'],['Obieg solarny','tempUpperSolar']])if(finite(c[key]))rows.push({name,key,kind:'tank'});
  return rows;
 }
 if(typeof module==='object'&&module.exports)module.exports={value,circuits};
 if(typeof document==='undefined')return;
 let loading=null,svg=null,latest=null,retryAfter=0,lastCircuits=[],lastModel='';
 function renderDetected(host,readings,data,fresh){
  const model=data.system?.controllerID||data.observed_controller||'ecoMAX',c=fresh?data.curr||{}:{};
  if(model!==lastModel){lastCircuits=[];lastModel=model;}
  if(fresh)lastCircuits=circuits(data);
  const rows=lastCircuits,height=Math.max(300,100+rows.length*132),ns='http://www.w3.org/2000/svg';
  const drawing=document.createElementNS(ns,'svg');drawing.setAttribute('viewBox',`0 0 900 ${height}`);drawing.classList.add('schema-detected');drawing.setAttribute('aria-label','Podgląd obiegów '+model);
  const node=(tag,attrs,parent=drawing,text)=>{const el=document.createElementNS(ns,tag);for(const [k,v]of Object.entries(attrs))el.setAttribute(k,String(v));if(text!==undefined)el.textContent=text;parent.append(el);return el;};
  node('text',{x:30,y:30,class:'schema-title'},drawing,model);
  node('text',{x:30,y:53,class:'schema-note'},drawing,'Podgląd odczytanych obiegów');
  const boilerY=height/2-55;
  if(rows.length){node('path',{d:`M 240 ${boilerY+55} H 300 V 144 M 300 ${144+(rows.length-1)*132} V 144`,class:'schema-connection'});}
  const box=(x,y,w,h)=>node('rect',{x,y,width:w,height:h,rx:14,class:'schema-box'});
  box(30,boilerY,210,150);
  node('text',{x:50,y:boilerY+30,class:'schema-label'},drawing,'Kocioł');
  node('text',{x:50,y:boilerY+67,class:'schema-temperature'},drawing,value(c.tempCO));
  node('text',{x:50,y:boilerY+94,class:'schema-note'},drawing,'Zadana: '+value(c.tempCOSet));
  node('text',{x:50,y:boilerY+123,class:'schema-note'},drawing,fresh?EcoSettings.boilerModeLabel(c.mode,data.write_transport):'Brak aktualnego odczytu');
  for(let i=0;i<rows.length;i++){
   const r=rows[i],y=84+i*132,state=typeof c[r.pump]==='boolean'?(c[r.pump]?'on':'off'):'unknown';
   node('path',{d:`M 300 ${y+60} H 375`,class:'schema-connection'});
   box(375,y,490,112);const group=node('g',{'data-circuit':r.key,'data-pump-state':state});
   node('text',{x:395,y:y+29,class:'schema-label'},group,r.name);
   node('text',{x:395,y:y+68,class:'schema-temperature'},group,value(c[r.key]));
   if(r.target)node('text',{x:395,y:y+94,class:'schema-note'},group,'Zadana: '+value(c[r.target]));
   if(r.pump){node('circle',{cx:705,cy:y+53,r:20,class:'schema-pump '+state},group);node('path',{d:`M 698 ${y+43} L 716 ${y+53} L 698 ${y+63} Z`,class:'schema-pump-arrow'},group);node('text',{x:735,y:y+57,class:'schema-note'},group,state==='on'?'Pompa pracuje':state==='off'?'Pompa wyłączona':'Stan pompy: ---');}
  }
  if(!rows.length)node('text',{x:375,y:height/2,class:'schema-note'},drawing,'Oczekiwanie na odczyty obiegów');
  host.hidden=false;host.replaceChildren(drawing);readings.replaceChildren();
  for(let i=1;i<=16;i++)if(finite(data.curr?.['ecoSterTemp'+i])){
   const row=document.createElement('div'),label=document.createElement('span'),reading=document.createElement('strong');label.textContent='Pomieszczenie '+i;reading.textContent=value(c['ecoSterTemp'+i]);row.append(label,reading);readings.append(row);
  }
 }
 function update(){
  if(!svg||!latest)return;
  const {data,fresh}=latest,c=fresh?data.curr||{}:{};
  const set=(id,text)=>{const node=svg.querySelector('#lbSchema_'+id);if(node)node.textContent=text;};
  set('model',data.system?.controllerID||'---');
  set('mode',fresh?EcoSettings.boilerModeLabel(c.mode,data.write_transport):'---');
  for(const [id,key]of [['tempExternalSensor','tempExternalSensor'],['ecoSterTemp1','ecoSterTemp1'],['tempFlueGas','tempFlueGas'],['tempCO','tempCO'],['tempCOSet','tempCOSet'],['tempBack','tempBack'],['tempCWU','tempCWU'],['tempCWUSet','tempCWUSet'],['mixerTempA','mixerTemp1'],['mixerTempASet','mixerSetTemp1']])set(id,value(c[key]));
  for(const [id,key,unit]of [['fuelLevel','fuelLevel','%'],['fuelStream','fuelStream','kg/h'],['fanPower','fanPower','%'],['tempOpticalSensor','tempOpticalSensor','']])set(id,id==='fuelLevel'&&c.fuelCalibration===true?'Kalibracja':value(c[key],unit));
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
  if(Number(data.system?.schemaID)!==23){renderDetected(host,readings,data,fresh);return;}
  host.hidden=false;
  if(svg){if(svg.parentNode!==host)host.replaceChildren(svg);update();return;}
  if(loading||Date.now()<retryAfter)return;
  host.textContent='Wczytywanie schematu...';
  loading=(async()=>{
   try{
    const response=await fetch('/assets/schema.svg',{cache:'force-cache'});if(!response.ok)throw new Error('HTTP '+response.status);
    const parsed=new DOMParser().parseFromString(await response.text(),'image/svg+xml');
    if(parsed.querySelector('parsererror')||parsed.documentElement.localName!=='svg')throw new Error('Niepoprawna grafika');
    svg=document.importNode(parsed.documentElement,true);
    if(Number(latest.data.system?.schemaID)===23){host.replaceChildren(svg);update();}
   }catch(error){retryAfter=Date.now()+30000;host.textContent='Nie uda\u0142o si\u0119 wczyta\u0107 schematu: '+error.message;}
   finally{loading=null;}
  })();
  await loading;
 }
 root.EcoHeatingSchema={render};
})(globalThis);
