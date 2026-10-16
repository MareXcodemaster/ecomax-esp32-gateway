'use strict';
(function(root){
 function calibrationRate(kg,seconds){
  if(typeof kg!=='number'||typeof seconds!=='number'||!Number.isFinite(kg)||!Number.isFinite(seconds)||kg<0.01||kg>25||seconds<10||seconds>3600)return null;
  const value=kg*3600/seconds;return value>=0.01&&value<=100?value:null;
 }
 function readings(data,fresh){
  const live=!!(fresh&&data?.fresh),finite=value=>typeof value==='number'&&Number.isFinite(value)?value:null;
  return {level:live?finite(data.level_percent):null,flow:live?finite(data.flow_kg_h):null,reported:live?finite(data.reported_flow_kg_h):null,consumed:fresh?finite(data?.consumed_kg):null,source:live?data.flow_source:'unavailable'};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={calibrationRate,readings};
 let snapshot=null,fresh=false,saving=false,revision=null,dirty=false;
 const get=id=>document.getElementById(id),num=(value,digits=2)=>typeof value==='number'&&Number.isFinite(value)?value.toLocaleString('pl-PL',{minimumFractionDigits:digits,maximumFractionDigits:digits}):'Brak odczytu';
 const quantity=(value,unit,digits=2)=>typeof value==='number'&&Number.isFinite(value)?num(value,digits)+' '+unit:'Brak odczytu';
 function preview(){
  const kg=get('pellet-mass').valueAsNumber,seconds=get('pellet-seconds').valueAsNumber,rate=calibrationRate(kg,seconds);
  get('pellet-rate-preview').textContent=quantity(rate,'kg/h');
  const current=snapshot?.calibration;
  get('pellet-save').disabled=saving||!fresh||!current||revision!==current.revision||rate===null;
  get('pellet-disable').disabled=saving||!fresh||!current||revision!==current.revision||current.feeder_kg_h===null;
 }
 function render(data,valid){
  snapshot=data;fresh=valid;const r=readings(data,valid);
  get('pellet-level').textContent=quantity(r.level,'%',0);get('pellet-flow').textContent=quantity(r.flow,'kg/h');
  get('pellet-consumed').textContent=quantity(r.consumed,'kg',3);
  get('pellet-flow-source').textContent=({boiler:'Strumie\u0144 zg\u0142aszany przez kocio\u0142',feeder_estimate:'Szacunek ESP / stan podajnika + pomiar wa\u017cony',unavailable:'Brak odczytu strumienia i aktywnego szacowania'})[r.source]||'Brak odczytu';
  get('pellet-reported-total').textContent=valid?quantity(data?.reported_consumed_kg,'kg',3):'Brak odczytu';
  get('pellet-estimated-total').textContent=valid?quantity(data?.estimated_consumed_kg,'kg',3):'Brak odczytu';
  get('pellet-counted').textContent=valid?quantity(data?.counted_seconds/3600,'h'): 'Brak odczytu';
  get('pellet-omitted').textContent=valid?quantity(data?.omitted_seconds/3600,'h'): 'Brak odczytu';
  get('pellet-feeder').textContent=valid&&data?.fresh&&typeof data.feeder_on==='boolean'?(data.feeder_on?'Pracuje':'Nie pracuje'):'Brak odczytu';
  get('pellet-calibration-status').textContent=data?.boiler_calibration_status||'Brak potwierdzonej komendy kalibracji zasobnika RS485';
  get('pellet-storage').textContent=!valid?'Brak po\u0142\u0105czenia z ESP':data?.storage_ok===false?'B\u0142\u0105d pami\u0119ci ESP':data?.restored_after_restart?'Licznik z pami\u0119ci ESP / przerwa restartu nie jest zliczona':'Licznik od uruchomienia tej wersji';
  const c=data?.calibration;if(c&&!dirty&&!saving){revision=c.revision;get('pellet-mass').value=c.sample_kg??'';get('pellet-seconds').value=c.sample_seconds??'';}
  get('pellet-draft-warning').textContent=dirty&&c&&revision!==c.revision?'Pomiar zmieniono poza formularzem. Odczytaj aktualny pomiar.':'';
  get('pellet-feeder-rate').textContent=valid?quantity(c?.feeder_kg_h,'kg/h'):'Brak odczytu';preview();
 }
 async function save(disable){
  if(saving||!fresh||!snapshot?.calibration||revision!==snapshot.calibration.revision)return;
  const kg=disable?0:get('pellet-mass').valueAsNumber,seconds=disable?0:get('pellet-seconds').valueAsNumber;
  if(!disable&&calibrationRate(kg,seconds)===null)return;
  saving=true;preview();get('pellet-result').textContent='';
  try{
   const updated=await request('/api/pellet/calibration',{revision,sample_kg:kg,sample_seconds:seconds});
   dirty=false;snapshot=updated;revision=updated.calibration.revision;
   get('pellet-result').textContent=disable?'Szacowanie podajnikiem wy\u0142\u0105czone. Nastawy kot\u0142a bez zmian.':'Pomiar zapisany w ESP. Kalibracja kot\u0142a bez zmian.';
  }catch(error){get('pellet-result').textContent=error.message+'. Nie ponowiono zapisu.';}
  finally{saving=false;render(snapshot,fresh);}
 }
 function init(){
  for(const id of ['pellet-mass','pellet-seconds'])get(id).addEventListener('input',()=>{dirty=true;preview();});
  get('pellet-calibration-form').onsubmit=event=>{event.preventDefault();save(false);};
  get('pellet-disable').onclick=()=>save(true);
  get('pellet-reload').onclick=()=>{dirty=false;render(snapshot,fresh);};
 }
 root.EcoPellet={init,render};
})(globalThis);
