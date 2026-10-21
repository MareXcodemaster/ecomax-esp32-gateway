'use strict';
(function(root){
 const roomModes=['Harmonogram','Ekonomiczny','Komfortowy','Wyj\u015bcie z domu','Wietrzenie','Party','Wakacje','Przeciwzamro\u017ceniowy'];
 const nativeModes=['Wy\u0142\u0105czony','Stabilizacja','Rozpalanie','Praca','Nadz\u00f3r','Przerwa','Post\u00f3j','Wygaszanie','Alarm','Praca r\u0119czna','Rozszczelnianie','Inny'];
 const cloudModes=['Wy\u0142\u0105czony','Stop','Rozpalanie','Praca','Nadz\u00f3r','Post\u00f3j','Czyszczenie','Wygaszanie','Alarm','R\u0119czny','Rozszczelnienie','Inny','Stabilizacja','Przedmuch','Kontrola p\u0142omienia','Zanik p\u0142omienia','Prewencja','Praca na ruszcie','Nadz\u00f3r na ruszcie','Kalibracja','Podtrzymanie','Dopalanie','Kominiarz','Rozgrzewanie','Otwarte drzwi','Ch\u0142odzenie','Bezpiecze\u0144stwo'];
 function boilerModeLabel(mode,route){const native=route==='rs485'||route==='hybrid';return (native?nativeModes:cloudModes)[native&&[12,23].includes(mode)?1:mode]||'Stan '+mode;}
 const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
 const clone=value=>JSON.parse(JSON.stringify(value));
 function roomMode(raw){return Number.isInteger(raw)&&raw>=0&&raw<=255&&(raw&127)<=7?raw&127:null;}
 function validSchedule(a){return Array.isArray(a)&&a.length===8&&a.slice(0,7).every(row=>Array.isArray(row)&&row.length===6&&row.every(n=>Number.isInteger(n)&&n>=0&&n<=255))&&Array.isArray(a[7])&&a[7].length===4&&a[7].every(Number.isInteger)&&[0,1].includes(a[7][0])&&a[7][2]>=0&&a[7][3]<=100&&a[7][2]<=a[7][1]&&a[7][1]<=a[7][3];}
 function encodeSlots(slots){if(!Array.isArray(slots)||slots.length!==48||slots.some(s=>typeof s!=='boolean'))throw new Error('Nieprawid\u0142owy dzie\u0144');return Array.from({length:6},(_,byte)=>slots.slice(byte*8,byte*8+8).reduce((n,on,i)=>n|(on?1<<(7-i):0),0));}
 const weekOrder=[1,2,3,4,5,6,0];
 const weekNames=['Poniedzia\u0142ek','Wtorek','\u015aroda','Czwartek','Pi\u0105tek','Sobota','Niedziela'];
 function paintWeekRange(value,from,to,full){
  const point=p=>Array.isArray(p)&&p.length===2&&Number.isInteger(p[0])&&p[0]>=0&&p[0]<7&&Number.isInteger(p[1])&&p[1]>=0&&p[1]<48;
  if(!validSchedule(value)||!point(from)||!point(to)||typeof full!=='boolean')throw new Error('Nieprawid\u0142owy przedzia\u0142 tygodnia');
  const result=clone(value);
  for(let row=Math.min(from[0],to[0]);row<=Math.max(from[0],to[0]);row++)for(let slot=Math.min(from[1],to[1]);slot<=Math.max(from[1],to[1]);slot++){
   const byte=slot>>3,mask=1<<(7-slot%8),day=weekOrder[row];result[day][byte]=full?result[day][byte]|mask:result[day][byte]&~mask;
  }
  return result;
 }
 function validValue(t,value){if(!Number.isFinite(value)||!Number.isFinite(t.step)||t.step<=0||value<t.min||value>t.max||Math.abs((value-t.min)/t.step-Math.round((value-t.min)/t.step))>0.0001)return false;if(t.value_kind==='boolean'&&![0,1].includes(value))return false;return !t.options?.length||t.options.some(o=>o.value===value);}
 function matchesTarget(a,b){return !!(a&&b&&['before','min','max','step','schema','advanced','cached','profile_mode','profile_raw','observed_key','wire_key'].every(k=>same(a[k],b[k])));}
 function reconcileSchedule(draft,actual){if(!draft)return null;return {...draft,changed:!same(draft.before,actual)};}
 function scheduleFresh(data,fresh,key){return !!(fresh&&(data.write_transport==='rs485'?(key==='thermostat1TZ'?data.thermostat_schedule_status:data.schedule_status)?.fresh:typeof data.system_age_seconds==='number'&&data.system_age_seconds<45));}
 function scheduleWritable(data,key,fresh){return scheduleFresh(data,fresh,key)&&validSchedule(data.schedules?.ecomaxSchedules?.[key])&&(data.write_transport!=='rs485'||Number.isInteger(data.schedule_write_indices?.[key]));}
 if(typeof module!=='undefined'&&module.exports)module.exports={roomModes,roomMode,validSchedule,encodeSlots,validValue,matchesTarget,reconcileSchedule,boilerModeLabel,scheduleFresh,scheduleWritable,weekOrder,paintWeekRange};
 if(typeof document==='undefined')return;
 let snap=null,isFresh=false,settingId=null,draft=null,scheduleDraft=null,scheduleDay=1,saving=false;
 let painting=null,paintFrame=0,weekEditable=false,weekFocus=0,weekSelection='',weekSource='',weekCells=[];
 const $=id=>document.getElementById(id),el=(tag,cls,text)=>{const v=document.createElement(tag);if(cls)v.className=cls;if(text!==undefined)v.textContent=text;return v;};
 const fmt=(v,unit='')=>typeof v==='number'?v.toLocaleString('pl-PL',{maximumFractionDigits:3})+(unit?' '+unit:''):'---';
 const label=(t,v)=>t?.options?.find(o=>o.value===v)?.label??(t?.value_kind==='boolean'?(v?'Tak':'Nie'):fmt(v,t?.unit));
 function open(id){
  if(!/^(rm:|legacy:)/.test(id)){EcoMirror.openTemperature(id);return;}
  const t=snap?.targets?.[id];if(!isFresh||!snap.targets_fresh||!t){notice('Brak aktualnego odczytu nastawy.',true);return;}
  settingId=id;draft={value:t.before,before:clone(t),dirty:false};$('setting-title').textContent=t.name;$('setting-error').textContent='';$('setting-risk').hidden=!t.risk;$('setting-confirm').checked=false;
  $('setting-risk').lastChild.textContent=t.advanced?'Potwierdzam zmian\u0119 ustawienia zaawansowanego kot\u0142a':'Potwierdzam zmian\u0119 nastawy instalacyjnej';
  const select=$('setting-option');select.replaceChildren(...(t.options||[]).map(o=>new Option(o.label,o.value)));select.hidden=t.value_kind!=='enum';$('setting-number').hidden=t.value_kind!=='number';$('setting-bool-field').hidden=t.value_kind!=='boolean';update();$('setting-dialog').showModal();
 }
 function update(){
  if(!settingId||!draft)return;const t=snap.targets?.[settingId],valid=isFresh&&snap.targets_fresh&&!!t;
  if(valid&&!draft.dirty){draft.value=t.before;draft.before=clone(t);}
  $('setting-before').textContent=(t?.cached?'Warto\u015b\u0107 z pami\u0119ci ecoNET: ':'Obecna nastawa: ')+(valid?label(t,t.before):'---');
  $('setting-cache').textContent=t?.cached?'Nieaktywny parametr: ecoNET nie udost\u0119pnia aktualnego odczytu zwrotnego. Przyj\u0119cie polecenia nie potwierdza nastawy.':'';
  $('setting-update').textContent='';
  $('setting-limits').textContent=valid&&t.value_kind==='number'?'Zakres: '+fmt(t.min,t.unit)+' - '+fmt(t.max,t.unit)+' / krok '+fmt(t.step,t.unit):'';
  const input=$('setting-number');if(document.activeElement!==input)input.value=draft.value;if(valid){input.min=t.min;input.max=t.max;input.step=t.step;}
  $('setting-option').value=draft.value;$('setting-bool').checked=!!draft.value;
  $('setting-save').disabled=!valid||!validValue(t,Number(draft.value))||actionBusy||snap.busy||saving||t.risk&&!$('setting-confirm').checked;
 }
 async function save(id,value,expected,installation=false){
  if(saving||actionBusy)throw new Error('Trwa zapis.');saving=true;
  try{
   const result=await run('/api/write/save',{id,value,confirm:true,confirm_installation:installation,confirm_advanced:installation&&expected.advanced===true});
   notice(result.message,!['verified','unchanged'].includes(result.state));return result;
  }finally{saving=false;}
 }
 function render(data,f){snap=data;isFresh=f;const t=data.targets?.['legacy:STER_MODE_1'];$('room-mode-value').textContent=f?(roomModes[roomMode(data.curr?.ecoSterMode1)]||'---'):'---';$('room-mode-edit').disabled=!f||!data.targets_fresh||!t||actionBusy||data.busy;$('room-mode-value').parentElement.hidden=!Number.isFinite(data.curr?.ecoSterMode1);if($('setting-dialog').open)update();}
 function scheduleData(data){const actual=data.schedules?.ecomaxSchedules||{};return scheduleDraft?{...actual,[scheduleDraft.key]:scheduleDraft.value}:actual;}
 function schedule(data,f,labels){
  snap=data;isFresh=f;const key=$('schedule-source').value,actual=data.schedules?.ecomaxSchedules?.[key];
  if(key!==weekSource){weekSource=key;weekSelection='';$('week-scroll').scrollLeft=0;weekCells[weekFocus].tabIndex=-1;weekFocus=0;weekCells[0].tabIndex=0;}
  const valid=scheduleWritable(data,key,f),readable=scheduleFresh(data,f,key),busy=actionBusy||data.busy||saving;
  $('schedule-room-mode-edit').hidden=key!=='thermostat1TZ';$('schedule-room-mode-edit').disabled=!f||!data.targets_fresh||!data.targets?.['legacy:STER_MODE_1']||busy;
  scheduleDraft=reconcileSchedule(scheduleDraft,actual);
  weekEditable=valid&&!busy&&!scheduleDraft?.changed;
  if(painting&&!weekEditable){finishPaint(true);scheduleDraft=reconcileSchedule(scheduleDraft,actual);}
  const value=scheduleDraft?.value||actual,decoded=(readable||scheduleDraft)&&validSchedule(value)?EcoMirror.decodeSchedule(value,0):null;
  $('schedule-source').disabled=!!scheduleDraft||busy;$('schedule-save').disabled=!weekEditable||!scheduleDraft||!validSchedule(scheduleDraft.value)||scheduleDraft.changed||busy||!!painting;$('schedule-reset').disabled=!scheduleDraft||busy||!!painting;
  $('schedule-enabled').disabled=!valid||busy;$('schedule-reduction-input').disabled=!valid||busy;
  if(decoded){$('schedule-reduction-input').min=decoded.min;$('schedule-reduction-input').max=decoded.max;if(document.activeElement!==$('schedule-reduction-input'))$('schedule-reduction-input').value=decoded.reduction;}
  $('schedule-edit-status').textContent=scheduleDraft?.changed?'Harmonogram zmieniono poza formularzem. Anuluj zmiany i odczytaj ponownie.':scheduleDraft?'Niezapisane zmiany':valid?'':readable?'Harmonogram tylko do odczytu':(key==='thermostat1TZ'?data.thermostat_schedule_status:data.schedule_status)?.error||'Brak aktualnego harmonogramu';
  $('week-grid').classList.toggle('stale',!readable);$('week-grid').setAttribute('aria-readonly',String(!weekEditable));
  for(let row=0;row<7;row++)for(let slot=0;slot<48;slot++){
   const cell=weekCells[row*48+slot],on=decoded?!!(value[weekOrder[row]][slot>>3]&(1<<(7-slot%8))):null;
   const selected=painting&&row>=Math.min(painting.from[0],painting.to[0])&&row<=Math.max(painting.from[0],painting.to[0])&&slot>=Math.min(painting.from[1],painting.to[1])&&slot<=Math.max(painting.from[1],painting.to[1]);
   cell.className='week-cell'+(on===null?' unavailable':on?' full':' reduced')+(selected?' painting':'');cell.disabled=!weekEditable;
   cell.setAttribute('aria-checked',String(on===true));cell.setAttribute('aria-label',weekNames[row]+', '+EcoMirror.clock(slot)+' - '+EcoMirror.clock(slot+1)+': '+(on===null?'Brak odczytu':on?labels.full:labels.reduced));cell.title=cell.getAttribute('aria-label');
  }
  $('schedule-selection').textContent=weekSelection;updateWeekViewport();
 }
 function gridPoint(cell){return [Number(cell.dataset.row),Number(cell.dataset.slot)];}
 function focusWeek(index){weekCells[weekFocus].tabIndex=-1;weekFocus=index;weekCells[index].tabIndex=0;weekCells[index].focus({preventScroll:true});}
 function selectionLabel(from,to){const first=Math.min(from[0],to[0]),last=Math.max(from[0],to[0]);return weekNames[first]+(first===last?'':' - '+weekNames[last])+', '+EcoMirror.clock(Math.min(from[1],to[1]))+' - '+EcoMirror.clock(Math.max(from[1],to[1])+1);}
 function previewPaint(point){
  if(!painting||!weekEditable)return;painting.to=point;const d=editSchedule();d.value=paintWeekRange(painting.base,painting.from,point,painting.full);scheduleDay=weekOrder[point[0]];weekSelection=selectionLabel(painting.from,point);refreshSchedule();
 }
 function finishPaint(cancel=false){
  if(!painting)return;const old=painting;painting=null;cancelAnimationFrame(paintFrame);paintFrame=0;
  if(cancel){scheduleDraft=old.previousDraft;weekSelection=old.previousSelection;}
  else if(scheduleDraft&&same(scheduleDraft.before,scheduleDraft.value))scheduleDraft=null;
  if($('week-grid').hasPointerCapture(old.id))$('week-grid').releasePointerCapture(old.id);
 }
 function movePaint(event){
  if(!painting||painting.id!==event.pointerId)return;
  painting.x=event.clientX;painting.y=event.clientY;painting.moved=true;
  const cell=document.elementFromPoint(event.clientX,event.clientY)?.closest('.week-cell');
  if(cell&&$('week-grid').contains(cell)){const point=gridPoint(cell);if(!same(point,painting.to))previewPaint(point);}
 }
 function scrollPaint(){
  if(!painting)return;const scroll=$('week-scroll'),rect=scroll.getBoundingClientRect(),label=$('week-grid').querySelector('th').getBoundingClientRect().width;
  if(painting.moved&&painting.y>=rect.top&&painting.y<=rect.bottom){
   const direction=painting.x>rect.right-25?1:painting.x<rect.left+label+25?-1:0,previous=scroll.scrollLeft;
   scroll.scrollLeft+=direction*5;
   if(previous!==scroll.scrollLeft){const cell=document.elementFromPoint(painting.x,painting.y)?.closest('.week-cell');if(cell&&scroll.contains(cell))previewPaint(gridPoint(cell));}
  }
  paintFrame=requestAnimationFrame(scrollPaint);
 }
 function updateWeekViewport(){
  const scroll=$('week-scroll'),table=$('week-grid'),width=table.getBoundingClientRect().width,label=table.querySelector('th').getBoundingClientRect().width;
  if(!width||!scroll.clientWidth)return;
  const step=(width-label)/48,first=Math.max(0,Math.min(47,Math.floor(scroll.scrollLeft/step))),last=Math.min(48,Math.ceil((scroll.scrollLeft+scroll.clientWidth-label)/step));
  $('week-navigation').hidden=scroll.scrollWidth<=scroll.clientWidth+1;$('week-visible-hours').textContent=EcoMirror.clock(first)+' - '+EcoMirror.clock(last);
  $('week-earlier').disabled=scroll.scrollLeft<1;$('week-later').disabled=scroll.scrollLeft+scroll.clientWidth>=scroll.scrollWidth-1;
 }
 function initWeek(){
  const table=$('week-grid'),thead=el('thead'),header=el('tr'),corner=el('th','','Dzie\u0144'),hours=el('th'),axis=el('div','week-hours');corner.scope='col';hours.colSpan=48;hours.setAttribute('aria-label','Godziny od 00:00 do 24:00');
  for(let hour=0;hour<=24;hour++){const tick=el('span','',String(hour).padStart(2,'0'));tick.style.left=(hour/24*100)+'%';axis.append(tick);}
  hours.append(axis);header.append(corner,hours);thead.append(header);const body=el('tbody');
  for(let row=0;row<7;row++){
   const tr=el('tr'),name=el('th','',weekNames[row]);name.scope='row';tr.dataset.day=weekOrder[row];tr.append(name);
   for(let slot=0;slot<48;slot++){const td=el('td'),button=el('button','week-cell unavailable');button.type='button';button.dataset.row=row;button.dataset.slot=slot;button.setAttribute('role','checkbox');button.tabIndex=row===0&&slot===0?0:-1;button.disabled=true;td.append(button);tr.append(td);weekCells.push(button);}
   body.append(tr);
  }
  table.append(thead,body);
  table.onpointerdown=event=>{
   const cell=event.target.closest('.week-cell');if(!cell||!weekEditable||event.button!==0||!event.isPrimary||painting)return;
   event.preventDefault();focusWeek(weekCells.indexOf(cell));const point=gridPoint(cell),value=scheduleDraft?.value||snap.schedules.ecomaxSchedules[$('schedule-source').value];
   painting={id:event.pointerId,from:point,to:point,base:clone(value),full:cell.getAttribute('aria-checked')!=='true',previousDraft:scheduleDraft?clone(scheduleDraft):null,previousSelection:weekSelection,x:event.clientX,y:event.clientY,moved:false};
   table.setPointerCapture(event.pointerId);previewPaint(point);paintFrame=requestAnimationFrame(scrollPaint);
  };
  table.onpointermove=movePaint;table.onpointerup=event=>{if(painting?.id===event.pointerId){movePaint(event);finishPaint();refreshSchedule();}};
  table.onpointercancel=event=>{if(painting?.id===event.pointerId){finishPaint(true);refreshSchedule();}};
  table.onlostpointercapture=()=>{if(painting){finishPaint(true);refreshSchedule();}};
  table.onclick=event=>{
   const cell=event.target.closest('.week-cell');if(!cell||event.detail!==0||!weekEditable||painting)return;
   const point=gridPoint(cell),d=editSchedule();d.value=paintWeekRange(d.value,point,point,cell.getAttribute('aria-checked')!=='true');if(same(d.before,d.value))scheduleDraft=null;scheduleDay=weekOrder[point[0]];weekSelection=selectionLabel(point,point);focusWeek(weekCells.indexOf(cell));refreshSchedule();
  };
  table.onkeydown=event=>{
   const cell=event.target.closest('.week-cell');if(!cell)return;let [row,slot]=gridPoint(cell);
   if(event.key==='Escape'&&painting){event.preventDefault();finishPaint(true);refreshSchedule();return;}
   if(event.key==='ArrowLeft')slot--;else if(event.key==='ArrowRight')slot++;else if(event.key==='ArrowUp')row--;else if(event.key==='ArrowDown')row++;
   else if(event.key==='Home'){slot=0;if(event.ctrlKey)row=0;}else if(event.key==='End'){slot=47;if(event.ctrlKey)row=6;}else return;
   event.preventDefault();row=Math.max(0,Math.min(6,row));slot=Math.max(0,Math.min(47,slot));focusWeek(row*48+slot);weekCells[weekFocus].scrollIntoView({block:'nearest',inline:'nearest'});
  };
  $('week-scroll').addEventListener('scroll',updateWeekViewport,{passive:true});new ResizeObserver(updateWeekViewport).observe($('week-scroll'));
  const cancelPaint=()=>{if(painting){finishPaint(true);refreshSchedule();}};window.addEventListener('blur',cancelPaint);document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelPaint();});
  for(const [id,direction]of [['week-earlier',-1],['week-later',1]])$(id).onclick=()=>{const step=(table.getBoundingClientRect().width-table.querySelector('th').getBoundingClientRect().width)/48;$('week-scroll').scrollBy({left:direction*step*12,behavior:'smooth'});};
 }
 function editSchedule(){if(!scheduleDraft){const key=$('schedule-source').value,before=clone(snap.schedules.ecomaxSchedules[key]);scheduleDraft={key,before,value:clone(before),changed:false};}return scheduleDraft;}
 function refreshSchedule(){EcoMirror.render('schedule',snap,isFresh);}
 function init(){
  initWeek();
  const band=el('div','room-mode-controls');band.innerHTML='<div><strong>Tryb ecoSTER 1</strong><div id="room-mode-value" class="muted"></div></div><button id="room-mode-edit" type="button"><i data-lucide="settings-2"></i>Zmie&#324; tryb</button>';$('mirror-tiles').before(band);
  const dialog=el('dialog');dialog.id='setting-dialog';dialog.innerHTML='<form id="setting-form"><div class="section-heading"><h2 id="setting-title"></h2><button type="button" class="icon" id="setting-close" title="Anuluj" aria-label="Anuluj"><i data-lucide="x"></i></button></div><p id="setting-before"></p><p id="setting-cache" class="muted"></p><label for="setting-number" id="setting-label">Nowa nastawa</label><input id="setting-number" type="number"><select id="setting-option" aria-label="Nowa nastawa"></select><label class="check" id="setting-bool-field"><input id="setting-bool" type="checkbox">W&#322;&#261;czony</label><p id="setting-limits" class="muted"></p><p id="setting-update" class="setting-update" role="status"></p><label class="check" id="setting-risk"><input id="setting-confirm" type="checkbox">Potwierdzam zmian&#281; nastawy instalacyjnej</label><p id="setting-error" class="error" role="alert"></p><div class="actions"><button type="button" id="setting-cancel">Anuluj</button><button type="submit" class="primary" id="setting-save"><i data-lucide="save"></i>Zapisz</button></div></form>';document.body.append(dialog);
  const cancel=()=>{settingId=null;dialog.close();};$('setting-close').onclick=cancel;$('setting-cancel').onclick=cancel;dialog.addEventListener('cancel',()=>{settingId=null;});
  for(const id of ['setting-number','setting-option','setting-bool'])$(id).addEventListener(id==='setting-number'?'input':'change',()=>{if(draft){draft.value=id==='setting-bool'?($(id).checked?1:0):Number($(id).value);draft.dirty=true;update();}});
  $('setting-confirm').onchange=update;$('room-mode-edit').onclick=()=>open('legacy:STER_MODE_1');
  $('setting-form').onsubmit=async e=>{e.preventDefault();if(!settingId)return;const id=settingId,value=Number(draft.value),expected=clone(draft.before),confirmation=$('setting-confirm').checked;try{const result=await save(id,value,expected,confirmation);if(['verified','unchanged','acknowledged','observed_only','unverified'].includes(result.state))cancel();}catch(error){$('setting-error').textContent=error.message;}finally{update();}};
  const toolbar=el('div','schedule-actions');toolbar.innerHTML='<span id="schedule-edit-status" class="setting-update" role="status"></span><button id="schedule-room-mode-edit" class="icon" type="button" title="Zmie&#324; tryb ecoSTER" aria-label="Zmie&#324; tryb ecoSTER" hidden><i data-lucide="settings-2"></i></button><button id="schedule-reset" type="button"><i data-lucide="undo-2"></i>Anuluj zmiany</button><button id="schedule-save" class="primary" type="button"><i data-lucide="save"></i>Zapisz</button>';document.querySelector('[data-section=schedule]').append(toolbar);
  $('schedule-room-mode-edit').onclick=()=>open('legacy:STER_MODE_1');
  const reduction=el('input');reduction.id='schedule-reduction-input';reduction.type='number';reduction.step='1';reduction.setAttribute('aria-label','Obni\u017cenie temperatury');$('schedule-reduction').replaceChildren(reduction);
  document.querySelector('.schedule-heading>.status').remove();document.querySelector('[data-section=service] .status').textContent='Nastawy instalacyjne';
  $('schedule-enabled').onchange=()=>{editSchedule().value[7][0]=$('schedule-enabled').checked?1:0;refreshSchedule();};reduction.oninput=()=>{editSchedule().value[7][1]=Number(reduction.value);refreshSchedule();};
  $('schedule-reset').onclick=()=>{scheduleDraft=null;weekSelection='';refreshSchedule();};
  $('schedule-save').onclick=async()=>{if(!scheduleDraft||saving)return;const expected=clone(scheduleDraft);if(!validSchedule(expected.value)){notice('Nieprawid\u0142owy harmonogram.',true);return;}saving=true;try{const prepared=await run('/api/write/prepare',{id:'schedule:'+expected.key,before:expected.before,value:expected.value,day:scheduleDay});if(!same(prepared.before,expected.before))throw new Error('Harmonogram zmieniono poza formularzem.');const result=await run('/api/write/execute',{token:prepared.token,confirm:true});notice(result.message,!['verified','unchanged'].includes(result.state));if(['verified','unchanged'].includes(result.state))scheduleDraft=null;}catch(error){notice(error.message,true);}finally{saving=false;refreshSchedule();}};
  icons();
 }
 root.EcoSettings={init,render,open,schedule,scheduleData,boilerModeLabel,scheduleFresh,roomModes,roomMode};
})(globalThis);
