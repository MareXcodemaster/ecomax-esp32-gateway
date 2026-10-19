'use strict';
(function(root){
 const fields=[
  ['co','tempCO','degC'],['coSet','tempCOSet','degC'],['cwu','tempCWU','degC'],['cwuSet','tempCWUSet','degC'],
  ['outside','tempExternalSensor','degC'],['feederTemp','tempFeeder','degC'],['exhaust','tempFlueGas','degC'],
  ['room','ecoSterTemp1','degC'],['roomSet','ecoSterSetTemp1','degC'],['mixer','mixerTemp1','degC'],['mixerSet','mixerSetTemp1','degC'],
  ['power','boilerPower','%'],['fan','fanPower','%'],['fuel','fuelLevel','%'],['flow','fuelStream','kg/h'],
  ['kw','boilerPowerKW','kW'],['oxygen','lambdaLevel','%',0.1],
  ['pumpCO','pumpCOWorks','boolean'],['pumpCWU','pumpCWUWorks','boolean'],['feeder','feederWorks','boolean'],
  ['lighter','lighterWorks','boolean'],['fanOn','fanWorks','boolean'],['mixerPump','mixerPumpWorks1','boolean'],
  ['circulation','pumpCirculationWorks','boolean'],['alarm','alarmOutputWorks','boolean'],
  ['room2','ecoSterTemp2','degC'],['roomSet2','ecoSterSetTemp2','degC'],['room3','ecoSterTemp3','degC'],['roomSet3','ecoSterSetTemp3','degC'],
  ['mixer2','mixerTemp2','degC'],['mixerSet2','mixerSetTemp2','degC'],['mixerPump2','mixerPumpWorks2','boolean'],
  ['mixer3','mixerTemp3','degC'],['mixerSet3','mixerSetTemp3','degC'],['mixerPump3','mixerPumpWorks3','boolean'],
  ['mixer4','mixerTemp4','degC'],['mixerSet4','mixerSetTemp4','degC'],['mixerPump4','mixerPumpWorks4','boolean']
 ];
 const scalar=value=>typeof value==='boolean'||typeof value==='number'&&Number.isFinite(value)?value:null;
 const label=value=>typeof value==='string'?value.slice(0,240):'';
 const iso=seconds=>new Date(seconds*1000).toISOString();
 const archiveKeys=['ready','loading','restored','retention_days','interval_seconds','samples','oldest_epoch','newest_epoch','bad_records','dropped'];
 function archiveStatus(value){return Object.fromEntries(archiveKeys.map(key=>[key,scalar(value?.[key])]));}
 function snapshot(value){
  if(!value||typeof value!=='object'||!value.system||!value.curr)throw Error('Nie odczytano aktualnego raportu ESP.');
  const readings={mode:scalar(value.curr.mode)};
  for(const [key,input,unit,scale=1]of fields){const v=value.curr[input];readings[key]=unit==='boolean'?(typeof v==='boolean'?Number(v):null):typeof v==='number'&&Number.isFinite(v)?Math.round(v*scale*100)/100:null;}
  const setpoints=Object.fromEntries(Object.entries(value.targets||{}).map(([id,target])=>[id,{name:label(target.name),value:scalar(target.before),effective:scalar(target.effective),min:scalar(target.min),max:scalar(target.max),step:scalar(target.step),write_ready:target.write_ready===true}]));
  const parameters=(Array.isArray(value.menu)?value.menu:[]).map(item=>({id:typeof item.index==='string'?label(item.index):scalar(item.index),name:label(item.name),category:label(item.category),unit:label(item.unit_name),value:scalar(item.metadata?.value),min:scalar(item.metadata?.minv),max:scalar(item.metadata?.maxv),editable:!!item.metadata?.edit,locked:!!item.locked}));
  return {firmware:label(value.version),controller:{model:label(value.system.controllerID),module_a:label(value.system.moduleASoftVer),panel:label(value.system.modulePanelSoftVer),schema_id:scalar(value.system.schemaID)},
   read_at_utc:label(value.last_read_utc),fresh:value.fresh===true,settings_fresh:value.targets_fresh===true,targets_read_at_epoch:scalar(value.target_read_epoch),menu_read_at_epoch:scalar(value.menu_read_epoch),age_seconds:scalar(value.age_seconds),transport:['rs485','econet','hybrid'].includes(value.write_transport)?value.write_transport:null,readings,setpoints,parameters};
 }
 function settingEvent(event){
  if(event?.schema!==1||!['baseline','changes','observed'].includes(event.kind)||!Number.isInteger(event.time)||typeof event.event_id!=='string'||!/^\d+:\d+$/.test(event.event_id)||!event.values||typeof event.values!=='object'||Array.isArray(event.values))throw Error('Nieprawidlowy rekord historii nastaw.');
  const values=Object.create(null);
  for(const [key,value]of Object.entries(event.values)){if(!/^(menu:\d+|target:[A-Za-z0-9:_-]{1,128})$/.test(key)||!(value===null||typeof value==='number'&&Number.isFinite(value)))throw Error('Nieprawidlowa wartosc historii nastaw.');values[key]=value;}
  return {schema:1,time:event.time,event_id:event.event_id,kind:event.kind,transport:label(event.transport),model:label(event.model),module_a:label(event.module_a),panel:label(event.panel),targets_observed_epoch:scalar(event.targets_observed_epoch),menu_observed_epoch:scalar(event.menu_observed_epoch),menu_included:event.menu_included===true,values};
 }
 function cancelled(signal){if(signal?.aborted)throw Object.assign(Error('Anulowano pobieranie.'),{name:'AbortError'});}
 async function collect(read,{from,to,signal,onProgress=()=>{}}){
  if(!Number.isInteger(from)||!Number.isInteger(to)||from<1577836800||to>4102444800||to<=from||to-from>259200)throw Error('Wybierz zakres do 3 dni.');
  cancelled(signal);const start=snapshot(await read('/api/export',signal));cancelled(signal);
  let columns=null;const records=new Map(),settingsRecords=new Map(),settingsQuality=[],archives=[];const total=Math.ceil((to-from)/3600);let part=0;
  for(let at=from;at<to;){
   const end=Math.min(to,at+3600);cancelled(signal);onProgress(part,total);
   const data=await read('/api/history?'+new URLSearchParams({from:at,to:end,raw:1}),signal);cancelled(signal);
   if(data?.export_raw!==true)throw Error('ESP nie udostepnia pelnych probek JSON. Wymagana beta.18 lub nowsza.');
   if(!Array.isArray(data.columns)||!Array.isArray(data.rows)||data.columns.length>64||!data.columns.every(key=>typeof key==='string')||new Set(data.columns).size!==data.columns.length||data.columns[0]!=='time'||!data.columns.includes('native')||!data.columns.includes('mode')||!data.columns.includes('gapBefore'))throw Error('Nieprawidlowy format historii.');
   if(columns&&JSON.stringify(columns)!==JSON.stringify(data.columns))throw Error('Format historii zmienil sie podczas eksportu. Pobierz ponownie.');
   columns=data.columns;archives.push(archiveStatus(data.archive));
   const settings=data.settings_history;settingsQuality.push({ready:settings?.ready===true,dropped:scalar(settings?.dropped),bad_files:scalar(settings?.bad_files),pending:scalar(settings?.pending)});
   if(settings?.events){if(!Array.isArray(settings.events)||settings.events.length>3000)throw Error('Nieprawidlowy rozmiar historii nastaw.');
    for(const value of settings.events){const event=settingEvent(value);if(event.time<Math.floor(at/3600)*3600||event.time>end)throw Error('Nastawa poza zakresem porcji historii.');settingsRecords.set(event.event_id,event);if(settingsRecords.size>30000)throw Error('Przekroczono limit historii nastaw.');}
   }
   if(data.rows.length>1440)throw Error('Zbyt duza porcja historii.');
   for(const row of data.rows){
    if(!Array.isArray(row)||row.length!==columns.length||!row.every(v=>v===null||typeof v==='number'&&Number.isFinite(v))||!Number.isInteger(row[0])||row[0]<at||row[0]>end)throw Error('Nieprawidlowa probka historii.');
    records.set(row[0],row.slice());if(records.size>7000)throw Error('Przekroczono limit probek eksportu.');
   }
   part++;onProgress(part,total);at=end;
  }
  cancelled(signal);const finish=snapshot(await read('/api/export',signal));cancelled(signal);
  if(JSON.stringify(start.controller)!==JSON.stringify(finish.controller)||start.firmware!==finish.firmware)throw Error('Sterownik lub firmware zmienily sie podczas eksportu. Pobierz ponownie.');
  const rows=[...records.values()].sort((a,b)=>a[0]-b[0]),native=columns.indexOf('native'),gap=columns.indexOf('gapBefore');
  const settingsEvents=[...settingsRecords.values()].sort((a,b)=>a.time-b.time||Number(a.event_id.split(':')[1])-Number(b.event_id.split(':')[1]));
  for(let i=1;i<rows.length;i++)if(rows[i][0]-rows[i-1][0]>90||rows[i][native]!==rows[i-1][native])rows[i][gap]=1;
  const warnings=[];
  if(!start.fresh||!finish.fresh)warnings.push('Biezace odczyty lub nastawy nie sa w pelni aktualne.');
  if(!rows.length)warnings.push('Brak zapisanych probek w wybranym okresie.');
  if(archives.some(a=>a.ready!==true))warnings.push('Archiwum flash bylo niedostepne; czesc historii moze pochodzic tylko z RAM.');
  if(archives.some(a=>a.bad_records>0||a.dropped>0))warnings.push('ESP zglasza pominiete lub uszkodzone rekordy historii.');
  if(rows.length&&rows[0][0]>from+90)warnings.push('Historia nie obejmuje poczatku wybranego okresu.');
  if(rows.length&&to-rows.at(-1)[0]>90)warnings.push('Brak swiezych probek na koncu wybranego okresu.');
  if(rows.some(row=>row[gap]===1))warnings.push('Historia zawiera przerwy lub zmiany zrodla odczytu; oznaczono je polem gapBefore.');
  if(!start.settings_fresh||!finish.settings_fresh)warnings.push('Biezace nastawy moga byc nieaktualne.');
  if(!settingsEvents.length)warnings.push('Brak zarejestrowanej historii nastaw. Rejestr zaczyna dzialac od beta.18; starszych nastaw nie odtworzono.');
  else{
   if(settingsEvents[0].time>from||settingsEvents[0].kind!=='baseline')warnings.push('Nie jest znany komplet nastaw na poczatku wybranego okresu.');
   if(settingsEvents.at(-1).time<to-900||settingsEvents.some((e,i)=>i&&e.time-settingsEvents[i-1].time>900))warnings.push('W rejestrze nastaw sa okresy bez potwierdzonego odczytu.');
  }
  if(settingsQuality.some(s=>!s.ready||s.dropped>0||s.bad_files>0||s.pending>0))warnings.push('Rejestr nastaw zglasza niedostepnosc, pominiete/uszkodzone rekordy lub oczekujacy zapis.');
  return {format:'ecomax-combustion-analysis',schema:1,created_at_utc:new Date().toISOString(),range:{from_utc:iso(from),to_utc:iso(to),from_epoch:from,to_epoch:to},
   current_at_start:start,current_at_finish:finish,
   settings_history:{events:settingsEvents,event_count:settingsEvents.length,time_semantics:'observed_at; not exact command time',heartbeat_seconds:600,historical_backfill:false,
    first_observed_at_utc:settingsEvents.length?iso(settingsEvents[0].time):null,last_observed_at_utc:settingsEvents.length?iso(settingsEvents.at(-1).time):null,
    dictionary:{...Object.fromEntries(start.parameters.filter(p=>p.id!==null).map(p=>['menu:'+p.id,{name:p.name,unit:p.unit}])),...Object.fromEntries(Object.entries(start.setpoints).map(([id,p])=>['target:'+id,{name:p.name}]))},
    quality:{ready:settingsQuality.every(s=>s.ready),dropped:Math.max(0,...settingsQuality.map(s=>s.dropped||0)),bad_files:Math.max(0,...settingsQuality.map(s=>s.bad_files||0)),pending:Math.max(0,...settingsQuality.map(s=>s.pending||0))}},
   history:{columns,rows,sample_count:rows.length,units:{time:'unix_seconds_UTC',native:'1=RS485; 0=local_ecoNET',mode:'controller_code',gapBefore:'boolean',...Object.fromEntries(fields.map(([key,,unit])=>[key,unit]))},
    source_keys:Object.fromEntries(fields.map(([key,input])=>[key,input])),additional_downsampling:false,nominal_interval_seconds:{flash:60,ram:10},archive_at_start:archives[0],archive_at_finish:archives.at(-1)},
   limitations:['Brak danych ma wartosc null, nie zero.','current_at_start/finish to biezace nastawy. Historia jest osobno w settings_history i nie zostala odtworzona za okres przed uruchomieniem rejestru.','Rekord baseline ustala znane wartosci; changes zmienia tylko wymienione klucze; observed potwierdza kolejny odczyt. Czas oznacza obserwacje, nie dokladna chwile zmiany.','Krotkie przejscia moga nie byc zapisane w minutowym archiwum.','Raport nie zastepuje pomiaru analizatorem spalin i nie wyznacza automatycznie nastaw spalania.'],warnings};
 }
 const api={fields,snapshot,settingEvent,collect};
 if(typeof module==='object'&&module.exports)module.exports=api;
 root.EcoAnalysis=api;
 if(typeof document==='undefined')return;
 document.addEventListener('DOMContentLoaded',()=>{
  const get=id=>document.getElementById(id);let active=null;
  function command(id,title){const button=document.createElement('button'),icon=document.createElement('i'),text=document.createElement('span');button.type='button';button.id=id;icon.dataset.lucide='file-json';text.textContent=title;button.append(icon,text);return button;}
  get('export').before(command('analysis-export','Pobierz analize spalania JSON'));
  get('history-download').after(command('history-json','JSON do analizy'));
  icons();
  const open=()=>{get('analysis-status').textContent='';get('analysis-dialog').showModal();};
  get('analysis-export').onclick=open;get('history-json').onclick=open;
  const close=()=>{if(active)active.abort();get('analysis-dialog').close();};
  get('analysis-close').onclick=close;get('analysis-cancel').onclick=close;
  get('analysis-dialog').addEventListener('cancel',()=>{if(active)active.abort();});
  get('analysis-form').onsubmit=async event=>{
   event.preventDefault();if(active)return;const controller=new AbortController();active=controller;
   get('analysis-download').disabled=true;get('analysis-hours').disabled=true;get('analysis-status').textContent='Odczytywanie nastaw...';
   try{
    const to=Math.floor(Date.now()/1000),hours=Number(get('analysis-hours').value);
    const report=await collect((path,signal)=>request(path,undefined,signal),{from:to-hours*3600,to,signal:controller.signal,onProgress:(n,total)=>{get('analysis-status').textContent='Pobieranie historii: '+n+' / '+total+' porcji';}});
    const modes=Object.create(null),ni=report.history.columns.indexOf('native'),mi=report.history.columns.indexOf('mode');
    for(const row of report.history.rows)if(Number.isInteger(row[mi])){const transport=row[ni]===1?'rs485':'econet';if(!modes[transport])modes[transport]=Object.create(null);modes[transport][row[mi]]=EcoSettings.boilerModeLabel(row[mi],transport);}
    report.history.mode_labels=modes;
    const blob=new Blob([JSON.stringify(report,null,2)],{type:'application/json;charset=utf-8'}),url=URL.createObjectURL(blob),link=document.createElement('a');
    link.href=url;link.download='piec-analiza-'+new Date(to*1000).toISOString().slice(0,19).replaceAll(':','-')+'.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
    get('analysis-status').textContent='Zapisano JSON: '+report.history.sample_count+' probek.'+(report.warnings.length?' Uwagi: '+report.warnings.join(' '):'');
   }catch(error){if(error.name!=='AbortError')get('analysis-status').textContent='Nie pobrano pliku: '+error.message;}
   finally{if(active===controller)active=null;get('analysis-download').disabled=false;get('analysis-hours').disabled=false;}
  };
 },{once:true});
})(globalThis);
