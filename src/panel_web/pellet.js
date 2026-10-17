'use strict';
(function(root){
 function calibrationRate(kg,seconds){
  if(typeof kg!=='number'||typeof seconds!=='number'||!Number.isFinite(kg)||!Number.isFinite(seconds)||kg<0.01||kg>25||seconds<10||seconds>3600)return null;
  const value=kg*3600/seconds;return value>=0.01&&value<=100?value:null;
 }
 function level(value,fresh){return fresh&&typeof value==='number'&&Number.isFinite(value)&&value>=0&&value<=100?value:null;}
 function readings(data,fresh){
  const live=!!(fresh&&data?.fresh),finite=value=>typeof value==='number'&&Number.isFinite(value)?value:null;
  return {level:level(data?.level_percent,live),flow:live?finite(data.flow_kg_h):null,reported:live?finite(data.reported_flow_kg_h):null,consumed:fresh?finite(data?.consumed_kg):null,source:live?data.flow_source:'unavailable'};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={calibrationRate,readings,level};
 if(typeof document==='undefined')return;
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
 }
 root.EcoPellet={init(){},render,decorateLevelTile};
})(globalThis);
