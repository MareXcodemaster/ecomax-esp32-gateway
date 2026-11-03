'use strict';
(function(root){
 const finite=v=>typeof v==='number'&&Number.isFinite(v);
 const measured=[['Kocioł','tempCO'],['CWU','tempCWU'],['Zewnętrzna','tempExternalSensor'],['Spaliny','tempFlueGas'],['Podajnik','tempFeeder'],['Powrót','tempBack'],['Bufor — góra','tempUpperBuffer'],['Bufor — dół','tempLowerBuffer'],['Sprzęgło','tempHydraulicCoupler'],['Solary — góra','tempUpperSolar'],['Solary — dół','tempLowerSolar'],['Kominek','tempFireplace'],['Wymiennik','tempExchanger'],['Powietrze — wejście','tempAirIn'],['Powietrze — wyjście','tempAirOut']];
 function sensors(c){const rows=measured.filter(([,k])=>finite(c[k]));for(let i=1;i<=16;i++){if(finite(c['mixerTemp'+i]))rows.push(['Mieszacz '+i,'mixerTemp'+i]);if(finite(c['ecoSterTemp'+i]))rows.push(['Pomieszczenie '+i,'ecoSterTemp'+i]);}return rows;}
 if(typeof module==='object'&&module.exports)module.exports={sensors};
 if(typeof document==='undefined')return;
 const format=(v,unit='°C')=>finite(v)?v.toLocaleString('pl-PL',{minimumFractionDigits:unit==='%'?0:1,maximumFractionDigits:1})+(unit?' '+unit:''):'---';
 let equipment=null,loading=null,latest=null,cacheKey='',lastRows=[],lastSensors=[];
 const storageKey=model=>'ecomax-schema-circuits:'+location.host+':'+model;
 const preferences=model=>{try{return JSON.parse(localStorage.getItem(storageKey(model)))||{};}catch{return {};}};
 const emitter=(settings,index)=>['radiator','floor','generic'].includes(settings[index])?settings[index]:index===2?'floor':'radiator';
 async function artwork(){
  if(equipment)return equipment;
  if(!loading)loading=(async()=>{const response=await fetch('/assets/schema-equipment.svg',{cache:'force-cache'});if(!response.ok)throw Error('HTTP '+response.status);const doc=new DOMParser().parseFromString(await response.text(),'image/svg+xml');if(doc.querySelector('parsererror')||!doc.querySelector('#equipment-boiler'))throw Error('Niepoprawna grafika');equipment=doc.querySelector('defs');return equipment;})().finally(()=>{loading=null;});
  return loading;
 }
 function draw(entry,defs){
  const {host,readings,data,fresh,rows}=entry,model=data.system?.controllerID||data.observed_controller||'ecoMAX',c=fresh?data.curr||{}:{},ns='http://www.w3.org/2000/svg';
  if(model!==cacheKey){lastRows=[];lastSensors=[];cacheKey=model;}
  if(fresh){lastRows=rows;lastSensors=sensors(data.curr||{});}
  const mixers=lastRows.filter(r=>r.kind==='mixer'),tank=lastRows.find(r=>r.key==='tempCWU'),settings=preferences(model);
  const width=Math.max(1140,520+mixers.length*200+(tank?200:0));
  const svg=document.createElementNS(ns,'svg');svg.classList.add('schema-detected','schema-plant');svg.setAttribute('viewBox',`0 0 ${width} 640`);svg.setAttribute('aria-label','Schemat poglądowy kotła, CWU i odczytanych mieszaczy');
  svg.append(document.importNode(defs,true));
  const node=(tag,attrs,parent=svg,text)=>{const e=document.createElementNS(ns,tag);for(const [k,v]of Object.entries(attrs))e.setAttribute(k,String(v));if(text!==undefined)e.textContent=text;parent.append(e);return e;};
  const text=(x,y,t,cls='plant-note',parent=svg)=>node('text',{x,y,class:cls},parent,t);
  const pipe=(d,type,parent=svg)=>node('path',{d,class:'plant-pipe '+type},parent);
  const use=(name,x,y,w,h,parent=svg)=>node('use',{href:'#equipment-'+name,x,y,width:w,height:h},parent);
  const pill=(x,y,t,kind='sensor',parent=svg)=>{node('rect',{x,y:y-20,width:102,height:28,rx:14,class:'plant-pill '+kind},parent);node('text',{x:x+51,y,class:'plant-pill-text','text-anchor':'middle'},parent,t);};
  const pump=(x,y,key,parent)=>{const state=typeof c[key]==='boolean'?(c[key]?'on':'off'):'unknown';node('circle',{cx:x,cy:y,r:15,class:'plant-pump '+state},parent);node('path',{d:`M${x-7} ${y+7} L${x} ${y-9} L${x+7} ${y+7} Z`,class:'plant-pump-arrow'},parent);text(x+24,y+5,state==='on'?'ON':state==='off'?'OFF':'---','plant-pump-state',parent);return state;};
  const connection=(x,y,parent=svg)=>node('circle',{cx:x,cy:y,r:4,class:'plant-sensor-point'},parent);
  const circuit=(r)=>node('g',{'data-circuit':r.key,'data-pump-state':typeof c[r.pump]==='boolean'?(c[r.pump]?'on':'off'):'unknown'});
  text(20,33,model,'plant-model');text(20,58,'Schemat poglądowy','plant-small');
  text(270,33,'Zasilanie','plant-hot-label');pipe('M 367 27 H 397','hot');text(425,33,'Powrót','plant-cold-label');pipe('M 495 27 H 525','cold');
  const tiles=[['Stan kotła',fresh?EcoSettings.boilerModeLabel(c.mode,data.write_transport):'---','power'],['Moc kotła',finite(c.boilerPowerKW)?format(c.boilerPowerKW,'kW'):format(c.boilerPower,'%'),'flame'],['Poziom paliwa',c.fuelCalibration===true?'Kalibracja':format(c.fuelLevel,'%'),'cylinder'],['Strumień paliwa',format(c.fuelStream,'kg/h'),'move-right'],['Nadmuch',format(c.fanPower,'%'),'fan'],['Płomień',format(c.tempOpticalSensor,''),'sun'],['Sonda lambda',format(finite(c.lambdaLevel)?c.lambdaLevel/10:null,'%'),'activity'],['Podajnik',typeof c.feederWorks==='boolean'?(c.feederWorks?'Pracuje':'Postój'):'---','settings']];
  const icons={power:'M12 2v10 M6 5a9 9 0 1 0 12 0',flame:'M12 2c3 7 9 9 8 14a8 8 0 0 1-16 0c0-4 3-6 4-10 1 4 3 5 4 5 1-3 1-6 0-9Z',cylinder:'M3 6c0-5 18-5 18 0s-18 5-18 0v12c0 5 18 5 18 0V6', 'move-right':'M2 12h20m-6-6 6 6-6 6',fan:'M12 10c-3-9 5-10 7-6 2 4-3 7-7 6Zm2 3c9-3 10 5 6 7-4 2-7-3-6-7Zm-4 1c3 9-5 10-7 6-2-4 3-7 7-6ZM9 10C0 13-1 5 3 3c4-2 7 3 6 7Z',sun:'M12 7a5 5 0 1 0 0 10 5 5 0 0 0 0-10m0-5v2m0 16v2M2 12h2m16 0h2M5 5l2 2m10 10 2 2M5 19l2-2M17 7l2-2',activity:'M1 12h5l3-9 6 18 3-9h5',settings:'M2 8h20M2 16h20M6 4v16M18 4v16'};
  tiles.forEach(([label,val,icon],i)=>{const x=20+(i%2)*112,y=100+Math.floor(i/2)*125;node('path',{d:icons[icon],transform:`translate(${x+39} ${y}) scale(1.4)`,class:'plant-stat-icon'});text(x+56,y+61,label,'plant-stat-label').setAttribute('text-anchor','middle');text(x+56,y+91,val,'plant-stat-value').setAttribute('text-anchor','middle');});
  pipe('M 250 85 V 605','divider');
  // The actual hydraulic connections are illustrative; only measurements and pump states come from RS485.
  use('boiler',285,145,110,407);text(295,582,'Kocioł','plant-label');
  const end=tank?500+mixers.length*200:mixers.length?590+(mixers.length-1)*200+35:500;
  pipe(`M 390 418 H ${end}`,'hot');pipe(`M 390 521 H ${end}`,'cold');
  connection(390,418);pill(398,376,format(c.tempCO));pill(398,406,format(c.tempCOSet),'target');
  connection(355,226);pill(395,226,format(c.tempFlueGas));text(398,196,'Spaliny','plant-small');
  connection(425,521);pill(407,578,format(c.tempBack));text(411,601,'Powrót','plant-small');
  if(finite(data.curr?.tempExternalSensor)){text(600,63,'Na zewnątrz','plant-small');pill(600,88,format(c.tempExternalSensor),'outdoor');}
  if(finite(data.curr?.ecoSterTemp1)){use('room',815,45,40,28);pill(865,68,format(c.ecoSterTemp1));}
  for(let i=0;i<mixers.length;i++){
   const r=mixers[i],index=Number(r.key.replace('mixerTemp','')),cx=590+i*200,hot=cx-28,cold=cx+35,parent=circuit(r),type=emitter(settings,index);
   text(cx,119,'Mieszacz '+index,'plant-label',parent).setAttribute('text-anchor','middle');text(cx,143,type==='floor'?'Podłogówka':type==='radiator'?'Grzejniki':'Obieg grzewczy','plant-small',parent).setAttribute('text-anchor','middle');
   pipe(`M ${hot} 418 V 226`,'hot',parent);pipe(`M ${cold} 226 V 521`,'cold',parent);
   if(type==='radiator')use('radiator',cx-54,158,112,78,parent);
   else if(type==='floor'){
    node('path',{d:`M ${cx-62} 165 L ${cx+67} 165 L ${cx+84} 222 H ${cx-47} Z`,class:'plant-floor-base'},parent);
    pipe(`M ${hot} 226 V 186 Q ${hot} 179 ${hot+7} 179 H ${cx+47} Q ${cx+55} 179 ${cx+55} 187 Q ${cx+55} 195 ${cx+47} 195 H ${cx-11} Q ${cx-19} 195 ${cx-19} 202 Q ${cx-19} 210 ${cx-11} 210 H ${cold} V 226`,'floor',parent);
   }else{node('rect',{x:cx-53,y:164,width:108,height:62,rx:8,class:'plant-emitter'},parent);text(cx,200,'CO','plant-label',parent).setAttribute('text-anchor','middle');}
   connection(hot,286,parent);pill(cx-131,272,format(c[r.key]),'sensor',parent);pill(cx-131,302,format(c[r.target]),'target',parent);
   pump(hot,341,r.pump,parent);pipe(`M ${hot} 395 H ${cold}`,'blend',parent);
   node('rect',{x:hot-10,y:385,width:20,height:20,class:'plant-valve'},parent);node('path',{d:`M ${hot-10} 385 L ${hot+10} 405 M ${hot+10} 385 L ${hot-10} 405`,class:'plant-valve-lines'},parent);
  }
  if(tank){
   const x=end+25,parent=circuit(tank);text(x+55,268,'Zasobnik CWU','plant-label',parent).setAttribute('text-anchor','middle');
   pill(x+8,301,format(c.tempCWU),'sensor',parent);pill(x+8,331,format(c.tempCWUSet),'target',parent);
   pipe(`M ${end} 418 V 420 H ${x+7}`,'hot',parent);pipe(`M ${end} 521 V 516 H ${x+7}`,'cold',parent);
   use('tank',x,354,132,197,parent);pump(end-34,418,tank.pump,parent);connection(x+11,385,parent);
  }
  const legend=text(280,632,'Temperatura zmierzona · poniżej temperatura zadana. ON / OFF: odczyt pompy.','plant-small');
  if(!fresh)legend.textContent='Brak aktualnych odczytów — temperatury i stany pomp są nieznane.';
  const optionsKey=model+':'+mixers.map(r=>r.key).join(','),oldOptions=readings.querySelector('.schema-circuit-options');
  const keepOptions=oldOptions?.dataset.key===optionsKey?oldOptions:null;
  host.hidden=false;host.replaceChildren(svg);for(const child of Array.from(readings.children))if(child!==keepOptions)child.remove();
  for(const [label,key]of lastSensors){const box=document.createElement('div'),name=document.createElement('span'),reading=document.createElement('strong');box.dataset.sensor=key;name.textContent=label;reading.textContent=format(c[key]);box.append(name,reading);readings.insertBefore(box,keepOptions);}
  if(keepOptions)return;
  const panel=document.createElement('details');panel.className='schema-circuit-options';panel.dataset.key=optionsKey;const summary=document.createElement('summary');summary.textContent='Opis obiegów na schemacie';panel.append(summary);
  const help=document.createElement('p');help.textContent='Wybór zmienia tylko rysunek w tej przeglądarce. Dopasuj grzejniki i podłogówkę do podłączenia mieszaczy.';panel.append(help);
  for(const r of mixers){const index=Number(r.key.replace('mixerTemp','')),label=document.createElement('label'),select=document.createElement('select');label.textContent='Mieszacz '+index+' ';select.setAttribute('aria-label','Rodzaj obiegu mieszacza '+index);for(const [v,n]of [['radiator','Grzejniki'],['floor','Podłogówka'],['generic','Obieg grzewczy']]){const o=document.createElement('option');o.value=v;o.textContent=n;select.append(o);}select.value=emitter(settings,index);select.addEventListener('change',()=>{const p=preferences(model);p[index]=select.value;try{localStorage.setItem(storageKey(model),JSON.stringify(p));}catch{}if(latest)draw(latest,defs);});label.append(select);panel.append(label);}
  if(mixers.length)readings.append(panel);
 }
 async function render(host,readings,data,fresh,rows){const entry={host,readings,data,fresh,rows};latest=entry;try{const defs=await artwork();if(latest===entry)draw(entry,defs);}catch{if(latest===entry){host.textContent='Nie udało się wczytać grafiki schematu. Odśwież stronę.';host.hidden=false;}}}
 function cancel(){latest=null;}
 root.EcoSchemaPlant={render,cancel};
})(globalThis);
