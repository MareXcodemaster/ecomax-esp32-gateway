'use strict';
(function(root){
 function calibrationRate(kg,seconds){
  if(typeof kg!=='number'||typeof seconds!=='number'||!Number.isFinite(kg)||!Number.isFinite(seconds)||kg<0.01||kg>25||seconds<10||seconds>3600)return null;
  const value=kg*3600/seconds;return value>=0.01&&value<=100?value:null;
 }
 function level(value,fresh){return fresh&&typeof value==='number'&&Number.isFinite(value)&&value>=0&&value<=100?value:null;}
 function calibrationPayload(point,physical){if(![0,100].includes(point)||physical!==true)throw new Error('Potwierdz rzeczywisty poziom pelletu');return {point,confirm:true,physical_level_confirmed:true};}
 function readings(data,fresh){
  const live=!!(fresh&&data?.fresh),finite=value=>typeof value==='number'&&Number.isFinite(value)?value:null;
  return {level:level(data?.level_percent,live),flow:live?finite(data.flow_kg_h):null,reported:live?finite(data.reported_flow_kg_h):null,consumed:fresh?finite(data?.consumed_kg):null,source:live?data.flow_source:'unavailable'};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={calibrationRate,readings,level,calibrationPayload};
 if(typeof document==='undefined')return;
 const get=id=>document.getElementById(id);let available=false,saving=false,point=null,initialized=false;
 function gauge(element,value){
  element.classList.add('pellet-gauge');element.dataset.available=String(value!==null);
  element.style.setProperty('--pellet-level',(value??0)+'%');
  element.setAttribute('role','img');element.setAttribute('aria-label',value===null?'Poziom pelletu: brak odczytu':'Poziom pelletu: '+Math.round(value)+'%');
  if(!element.firstElementChild){const fill=document.createElement('span');fill.className='pellet-fill';element.append(fill);}
 }
 function decorateLevelTile(tile,value,fresh){
  if(!tile)return;
  const graphic=tile.querySelector('.pellet-gauge,.tile-symbol,.tile-bitmap');
  if(!graphic)return;
  if(!graphic.classList.contains('pellet-gauge')){graphic.className='pellet-gauge';graphic.replaceChildren();}
  value=level(value,fresh);gauge(graphic,value);tile.dataset.levelAvailable=String(value!==null);
  tile.querySelector('.tile-value').textContent=value===null?'Brak odczytu':Math.round(value)+' %';
 }
 function render(data,valid){
  const value=readings(data,valid).level;
  document.getElementById('pellet-level').textContent=value===null?'Brak odczytu':Math.round(value)+' %';
  gauge(document.getElementById('pellet-hopper'),value);
  document.getElementById('pellet-calibration-status').textContent=data?.boiler_calibration_status||'Brak potwierdzonej komendy kalibracji zasobnika RS485';
  available=!!(valid&&data?.boiler_calibration_available);
  for(const id of ['pellet-full','pellet-empty'])get(id).disabled=!available||saving;
  if(initialized)get('pellet-point-submit').disabled=!available||saving||!get('pellet-point-confirm').checked;
 }
 function init(){
  const dialog=document.createElement('dialog');dialog.id='pellet-point-dialog';dialog.innerHTML='<form id="pellet-point-form"><h2 id="pellet-point-title"></h2><p>To czynno&#347;&#263; kalibracji sterownika, nie wpisanie ilo&#347;ci kilogram&#243;w. Samo przyj&#281;cie polecenia nie potwierdza poprawno&#347;ci ca&#322;ej kalibracji.</p><label class="check"><input type="checkbox" id="pellet-point-confirm"><span id="pellet-point-label"></span></label><p id="pellet-point-error" class="error" role="alert"></p><div class="actions"><button id="pellet-point-cancel" type="button">Anuluj</button><button id="pellet-point-submit" class="primary" type="submit" disabled><i data-lucide="check"></i>Potwierd&#378; punkt</button></div></form>';document.body.append(dialog);
  const open=value=>{if(!available||saving)return;point=value;get('pellet-point-title').textContent='Poziom paliwa '+value+'%';get('pellet-point-confirm').checked=false;get('pellet-point-submit').disabled=true;get('pellet-point-error').textContent='';get('pellet-point-label').textContent=value===100?'Potwierdzam, \u017ce zasobnik jest rzeczywi\u015bcie pe\u0142ny':'Potwierdzam, \u017ce zasobnik jest rzeczywi\u015bcie pusty';dialog.showModal();};
  initialized=true;get('pellet-full').onclick=()=>open(100);get('pellet-empty').onclick=()=>open(0);
  get('pellet-point-cancel').onclick=()=>{if(!saving)dialog.close();};dialog.addEventListener('cancel',event=>{if(saving)event.preventDefault();});
  get('pellet-point-confirm').onchange=()=>{get('pellet-point-submit').disabled=!available||saving||!get('pellet-point-confirm').checked;};
  get('pellet-point-form').onsubmit=async event=>{
   event.preventDefault();if(!available||saving||!get('pellet-point-confirm').checked)return;
   saving=true;get('pellet-point-submit').disabled=true;get('pellet-point-cancel').disabled=true;
   try{const result=await run('/api/pellet/calibrate',calibrationPayload(point,true));notice(result.message,result.state!=='acknowledged');dialog.close();}
   catch(error){get('pellet-point-error').textContent=error.message+' Nie ponowiono polecenia. Sprawd\u017a ekran kot\u0142a przed kolejn\u0105 pr\u00f3b\u0105.';}
   finally{saving=false;get('pellet-point-cancel').disabled=false;get('pellet-point-confirm').checked=false;get('pellet-point-submit').disabled=true;}
  };
 }
 root.EcoPellet={init,render,decorateLevelTile};
})(globalThis);
