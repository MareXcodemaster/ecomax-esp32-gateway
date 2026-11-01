'use strict';
(function(root){
 const validWeek=week=>Array.isArray(week)&&week.length===7&&week.every(day=>Array.isArray(day)&&day.length===6&&day.every(v=>Number.isInteger(v)&&v>=0&&v<=255));
 function paint(week,from,to,on){
  const validPoint=p=>Array.isArray(p)&&p.length===2&&Number.isInteger(p[0])&&Number.isInteger(p[1])&&p[0]>=0&&p[0]<7&&p[1]>=0&&p[1]<48;
  if(!validWeek(week)||!validPoint(from)||!validPoint(to)||typeof on!=='boolean')throw new Error('Nieprawidlowy tydzien');
  const next=week.map(day=>day.slice());
  for(let row=Math.min(from[0],to[0]);row<=Math.max(from[0],to[0]);row++)for(let slot=Math.min(from[1],to[1]);slot<=Math.max(from[1],to[1]);slot++){
   const byte=slot>>3,bit=1<<(7-slot%8);next[row][byte]=on?next[row][byte]|bit:next[row][byte]&~bit;
  }
  return next;
 }
 function validSettings(value){return Number.isFinite(value.lower)&&Number.isFinite(value.upper)&&value.lower>=5&&value.upper<=85&&value.upper-value.lower>=.5&&[0,1,2].includes(value.mode)&&typeof value.schedule_enabled==='boolean'&&validWeek(value.week);}
 if(typeof module!=='undefined'&&module.exports)module.exports={paint,validWeek,validSettings};
 if(typeof document==='undefined')return;
 let snapshot=null,visible=false,busy=false,loading=false,dirty=false,lastRead=0,epoch=0,scanSequence=null,week=Array.from({length:7},()=>Array(6).fill(0)),painting=null,focus=0;
 const $=id=>document.getElementById(id),days=['Poniedzia\u0142ek','Wtorek','\u015aroda','Czwartek','Pi\u0105tek','Sobota','Niedziela'];
 const message=(value,error=false)=>{$('pump-message').textContent=value;$('pump-message').dataset.error=String(error);};
 function fields(){return {action:'settings',lower:Number($('pump-lower').value),upper:Number($('pump-upper').value),mode:1,schedule_enabled:$('pump-schedule-enabled').checked,week};}
 function quickMode(mode){
  if(busy||!snapshot?.paired)return;
  const value={action:'settings',lower:snapshot.lower,upper:snapshot.upper,mode,schedule_enabled:!!snapshot.schedule_enabled,week:snapshot.week?.map(row=>row.slice())};
  if(validSettings(value))send(value,true);
 }
 function tapState(value){
  $('pump-tap').dataset.state=value;
  $('pump-tap').setAttribute('aria-label',value==='on'?'Cyrkulacja włączona':value==='off'?'Cyrkulacja wyłączona':'Stan cyrkulacji nieznany');
 }
 function grid(){
  const active=$('pump-schedule-enabled').checked;
  $('pump-schedule-panel').hidden=!active;
  for(const cell of $('pump-week').querySelectorAll('button')){
   const day=Number(cell.dataset.row),slot=Number(cell.dataset.slot),on=!!(week[day][slot>>3]&(1<<(7-slot%8)));
   cell.className='week-cell '+(on?'full':'reduced');cell.setAttribute('aria-checked',String(on));
   cell.setAttribute('aria-label',days[day]+' '+EcoMirror.clock(slot)+' - '+EcoMirror.clock(slot+1)+': '+(on?'dozwolona praca':'przerwa'));
   cell.disabled=busy;cell.tabIndex=day*48+slot===focus?0:-1;
  }
  $('pump-save').disabled=busy||!snapshot?.paired;
  for(const id of ['pump-upper','pump-lower','pump-host','pump-username','pump-password','pump-connect-save','pump-device-select','pump-reset'])$(id).disabled=busy;
  $('pump-connect-save').disabled=busy||!!snapshot?.scan_busy;$('pump-device-select').disabled=busy||!!snapshot?.scan_busy;
  $('pump-schedule-enabled').disabled=busy;
  $('pump-enabled').disabled=busy;
  for(const [id,mode] of [['pump-off',0],['pump-auto',1],['pump-start',2]]){
   const button=$(id);button.disabled=busy||!snapshot?.paired;
   button.setAttribute('aria-pressed',String(!!snapshot?.armed&&snapshot.mode===mode));
  }
 }
 function render(data){
  snapshot=data;
  $('pump-enabled').checked=!!data.enabled;
  $('pump-content').hidden=!data.enabled;
  $('pump-feature-hint').textContent=data.enabled?(data.paired?'ESP steruje pompą samodzielnie, bez otwartej strony.':'Połącz gniazdko Tapo w ustawieniach poniżej, a następnie wybierz tryb pracy.'):'Opcja wyłączona. ESP nie wysyła poleceń do pompy; stan gniazdka pozostaje bez zmian.';
  $('pump-device-name').textContent=data.alias||'Pompa cyrkulacyjna';
  tapState(data.fresh&&typeof data.on==='boolean'?(data.on?'on':'off'):'unknown');
  $('pump-on').textContent=data.fresh&&typeof data.on==='boolean'?(data.on?'Pracuje':'Wy\u0142\u0105czona'):'Stan nieznany';
  $('pump-temperature').textContent=typeof data.temperature==='number'?data.temperature.toLocaleString('pl-PL',{minimumFractionDigits:1,maximumFractionDigits:1})+' \u00b0C':'Brak odczytu';
  $('pump-mode-name').textContent=!data.armed?'Wybierz tryb':({0:'Wyłączona',1:'Automatycznie',2:'Ręcznie'}[data.mode]||'Stan nieznany');
  $('pump-reason').textContent=!data.paired?'Najpierw połącz gniazdko Tapo.':!data.armed?'Połączono. Wybierz Automatycznie, Ręcznie lub Wyłączona.':data.reason||'';
  $('pump-link').textContent=data.status||'Brak konfiguracji';
  $('pump-device').textContent=[data.protocol,data.model,data.host,data.mac].filter(Boolean).join(' / ');
  $('pump-rules-summary').textContent='Auto: włącz od '+data.upper+' °C, wyłącz po spadku do '+data.lower+' °C. '+(data.schedule_enabled?'Tylko w zaznaczonych godzinach.':'Bez ograniczenia godzin.');
  $('pump-credentials-note').hidden=!data.credentials_need_refresh;
  if(scanSequence!==null&&data.scan_sequence!==scanSequence&&!data.scan_busy)message('Znaleziono gniazdek Tapo: '+(data.devices||[]).length+'.');
  scanSequence=data.scan_sequence??null;
  $('pump-scan').disabled=busy||data.scan_busy;$('pump-scan').querySelector('span').textContent=data.scan_busy?'Wyszukiwanie...':'Wyszukaj gniazdka';
  const list=$('pump-device-select'),signature=JSON.stringify(data.devices||[]),previous=list.value;
  if(list.dataset.signature!==signature){list.replaceChildren(new Option('Wybierz urz\u0105dzenie',''));for(const device of data.devices||[])list.append(new Option(device.model+' / '+device.host+' / '+device.mac+' / '+device.protocol,device.mac));list.dataset.signature=signature;list.value=previous;}
  list.hidden=!(data.devices||[]).length;
  if(!dirty&&!busy){
   $('pump-lower').value=data.lower??40;$('pump-upper').value=data.upper??45;
   $('pump-schedule-enabled').checked=!!data.schedule_enabled;if(validWeek(data.week))week=data.week.map(row=>row.slice());
  }
  if(!$('pump-host').value&&data.host)$('pump-host').value=data.host;
  if(!$('pump-mac').value&&data.mac)$('pump-mac').value=data.mac;
  grid();
 }
 async function refresh(force=false){
  if(!visible||loading||busy||(!force&&Date.now()-lastRead<1500))return;
  loading=true;const started=epoch;try{const result=await request('/api/cwu-pump');if(started===epoch&&!busy){render(result);lastRead=Date.now();}}catch(error){if(started===epoch&&!busy){$('pump-link').textContent='Brak po\u0142\u0105czenia z ESP';$('pump-on').textContent='Stan nieznany';tapState('unknown');}}finally{loading=false;}
 }
 async function send(payload,quick=false){
  if(busy)return;busy=true;epoch++;grid();message('');
  payload.request_id=Array.from(crypto.getRandomValues(new Uint8Array(16)),v=>v.toString(16).padStart(2,'0')).join('');
  try{const result=await request('/api/cwu-pump',payload);if(payload.action==='settings'&&!quick)dirty=false;render(result);message(payload.action==='feature'?(result.enabled?'Obsługa cyrkulacji włączona.':'Obsługa cyrkulacji wyłączona. Stan gniazdka nie został zmieniony.'):payload.action==='scan'?'Wyszukiwanie urz\u0105dze\u0144...':payload.action==='connect'?'Dane przyj\u0119te. Oczekiwanie na odczyt gniazdka.':quick?'Tryb zapisany. Oczekiwanie na potwierdzenie stanu gniazdka.':'Ustawienia zapisane. Automatyka jest włączona.');}
  catch(error){message(error.message,true);}
  finally{if(payload.password)payload.password='';if(payload.action==='connect')$('pump-password').value='';busy=false;if(snapshot)$('pump-enabled').checked=!!snapshot.enabled;grid();refresh(true);}
 }
 function init(){
  const section=document.createElement('section');section.dataset.section='pump';section.hidden=true;
  section.innerHTML="\n<div class=\"pump-feature\"><label class=\"check\"><input id=\"pump-enabled\" type=\"checkbox\">Włącz obsługę cyrkulacji CWU</label><p id=\"pump-feature-hint\">Opcjonalne sterowanie pompą przez gniazdko Tapo P100, P105, P110, P110M lub P115 (KLAP / TPAP).</p></div>\n<div id=\"pump-content\" hidden>\n<div class=\"pump-overview\">\n<div class=\"pump-hero\">\n<svg id=\"pump-tap\" class=\"pump-tap\" data-state=\"unknown\" viewBox=\"0 0 200 192\" role=\"img\" aria-label=\"Stan cyrkulacji nieznany\">\n<ellipse cx=\"100\" cy=\"178\" rx=\"72\" ry=\"8\" fill=\"#e1eeeb\"/>\n<g class=\"tap-handle\" stroke=\"currentColor\" stroke-width=\"9\" stroke-linecap=\"round\"><path d=\"M77 44h46\"/><path d=\"M100 44v24\"/></g>\n<path d=\"M24 98h51V78h50v20h17c22 0 34 12 34 34v5h-28v-7c0-5-3-8-8-8H24z\" fill=\"#d6e9e5\" stroke=\"currentColor\" stroke-width=\"7\" stroke-linejoin=\"round\"/>\n<path d=\"M26 88v45M80 150h46\" stroke=\"currentColor\" stroke-width=\"8\" stroke-linecap=\"round\"/>\n<g class=\"tap-water\" fill=\"#2896ce\"><path class=\"tap-drop drop-one\" d=\"M154 143c-2 5-5 8-5 11a5 5 0 0 0 10 0c0-3-3-6-5-11z\"/><path class=\"tap-drop drop-two\" d=\"M164 143c-2 5-5 8-5 11a5 5 0 0 0 10 0c0-3-3-6-5-11z\"/><path class=\"tap-drop drop-three\" d=\"M174 143c-2 5-5 8-5 11a5 5 0 0 0 10 0c0-3-3-6-5-11z\"/></g>\n<g class=\"tap-unknown\"><circle cx=\"162\" cy=\"159\" r=\"16\" fill=\"#fff\" stroke=\"#7a858e\" stroke-width=\"2\"/><text x=\"162\" y=\"165\" text-anchor=\"middle\" font-size=\"21\" fill=\"#68727c\">?</text></g>\n</svg>\n<div><h3>Pompa cyrkulacyjna</h3><p id=\"pump-device-name\" class=\"pump-alias\"></p><strong id=\"pump-on\">Stan nieznany</strong><p id=\"pump-link\">Nie skonfigurowano</p></div>\n</div>\n<dl class=\"pump-status\"><div><dt>Temperatura CWU</dt><dd id=\"pump-temperature\">Brak odczytu</dd></div><div><dt>Tryb sterowania</dt><dd id=\"pump-mode-name\">Wybierz tryb</dd><small id=\"pump-reason\"></small></div></dl>\n</div>\n<div class=\"pump-controls\" role=\"group\" aria-label=\"Sterowanie pompą cyrkulacyjną\"><button id=\"pump-auto\" type=\"button\"><i data-lucide=\"thermometer\"></i>Automatycznie</button><button id=\"pump-start\" type=\"button\"><i data-lucide=\"power\"></i>Ręcznie</button><button id=\"pump-off\" type=\"button\"><i data-lucide=\"circle-stop\"></i>Wyłączona</button></div>\n<p id=\"pump-rules-summary\" class=\"pump-note\"></p><p class=\"pump-caption\">Ręcznie: pompa włączona bez progów i harmonogramu. Wyłączona: pompa zatrzymana.</p><p class=\"pump-caption\">Animacja pokazuje potwierdzone zasilanie pompy. Gniazdko nie mierzy przepływu wody.</p>\n<details class=\"pump-rules\"><summary>Ustawienia automatyczne i harmonogram</summary>\n<form id=\"pump-settings\" class=\"pump-settings\"><div class=\"pump-fields pump-thresholds\"><label>Włącz od temperatury CWU [°C]<input id=\"pump-upper\" type=\"number\" min=\"5.5\" max=\"85\" step=\"0.5\" value=\"45\" required></label><label>Wyłącz po spadku do [°C]<input id=\"pump-lower\" type=\"number\" min=\"5\" max=\"84.5\" step=\"0.5\" value=\"40\" required></label></div><label class=\"check\"><input id=\"pump-schedule-enabled\" type=\"checkbox\">Używaj harmonogramu</label>\n<div id=\"pump-schedule-panel\" hidden><div class=\"week-navigation\"><button id=\"pump-earlier\" class=\"icon\" type=\"button\" aria-label=\"Wcześniejsze godziny\" title=\"Wcześniejsze godziny\"><i data-lucide=\"chevron-left\"></i></button><button id=\"pump-later\" class=\"icon\" type=\"button\" aria-label=\"Późniejsze godziny\" title=\"Późniejsze godziny\"><i data-lucide=\"chevron-right\"></i></button></div><div class=\"week-scroll\" id=\"pump-week-scroll\"><table id=\"pump-week\" class=\"week-grid\" aria-label=\"Godziny dozwolonej pracy pompy\"></table></div><div class=\"schedule-legend\"><span><i class=\"full\"></i>Dozwolona praca</span><span><i class=\"reduced\"></i>Przerwa</span></div></div>\n<div class=\"actions\"><button id=\"pump-reset\" type=\"button\"><i data-lucide=\"undo-2\"></i>Anuluj zmiany</button><button id=\"pump-save\" class=\"primary\" type=\"submit\"><i data-lucide=\"save\"></i>Zapisz i włącz automatykę</button></div></form></details>\n<details class=\"pump-connect\" id=\"pump-connect\"><summary>Połączenie Tapo</summary><p id=\"pump-device\" class=\"pump-note\"></p><div class=\"pump-discovery\"><button type=\"button\" id=\"pump-scan\"><i data-lucide=\"search\"></i><span>Wyszukaj gniazdka</span></button><select id=\"pump-device-select\" aria-label=\"Znalezione gniazdka Tapo\" hidden></select></div><form id=\"pump-connect-form\"><div class=\"pump-fields\"><label>Adres IP gniazdka<input id=\"pump-host\" type=\"text\" inputmode=\"decimal\" placeholder=\"192.168.1.100\" required autocomplete=\"off\"></label><label>Konto Tapo<input id=\"pump-username\" type=\"email\" autocomplete=\"off\"></label><label>Hasło Tapo<input id=\"pump-password\" type=\"password\" autocomplete=\"off\" required></label></div><input id=\"pump-mac\" type=\"hidden\"><div class=\"actions\"><button type=\"submit\" id=\"pump-connect-save\"><i data-lucide=\"plug-zap\"></i>Połącz lokalnie</button></div></form></details>\n</div><p id=\"pump-message\" role=\"status\"></p>\n";
  const main=document.querySelector('main');main.insertBefore(section,main.querySelector('footer'));
  $('pump-username').required=false;
  $('pump-username').parentElement.firstChild.textContent='E-mail Tapo (opcjonalny dla TPAP)';
  const credentialsNote=document.createElement('p');credentialsNote.id='pump-credentials-note';credentialsNote.className='pump-note';credentialsNote.hidden=true;
  credentialsNote.textContent='Po aktualizacji wpisz raz ponownie has\u0142o Tapo i wybierz Po\u0142\u0105cz lokalnie, aby uruchomi\u0107 TPAP.';
  $('pump-connect-form').before(credentialsNote);
  const supported=document.createElement('p');supported.className='pump-note';supported.textContent='Gniazdka: Tapo P100, P105, P110, P110M i P115 z KLAP / TPAP. Starszy protokół AES i inne marki nie są obsługiwane.';
  $('pump-connect-form').before(supported);
  const table=$('pump-week'),head=document.createElement('thead'),header=document.createElement('tr'),corner=document.createElement('th'),hours=document.createElement('th'),axis=document.createElement('div');
  corner.textContent='Dzie\u0144';corner.scope='col';hours.colSpan=48;axis.className='week-hours';
  for(let hour=0;hour<=24;hour++){const tick=document.createElement('span');tick.textContent=String(hour).padStart(2,'0');tick.style.left=hour/24*100+'%';axis.append(tick);}
  hours.append(axis);header.append(corner,hours);head.append(header);table.append(head);const body=document.createElement('tbody');
  for(let day=0;day<7;day++){const row=document.createElement('tr'),label=document.createElement('th');label.scope='row';label.textContent=days[day];row.append(label);for(let slot=0;slot<48;slot++){const td=document.createElement('td'),button=document.createElement('button');button.type='button';button.dataset.row=day;button.dataset.slot=slot;button.setAttribute('role','checkbox');td.append(button);row.append(td);}body.append(row);}table.append(body);
  const point=cell=>[Number(cell.dataset.row),Number(cell.dataset.slot)];
  function apply(to){week=paint(painting.base,painting.from,to,painting.on);dirty=true;grid();}
  table.onpointerdown=event=>{const cell=event.target.closest('button');if(!cell||busy||event.button!==0||!event.isPrimary)return;event.preventDefault();focus=Number(cell.dataset.row)*48+Number(cell.dataset.slot);painting={id:event.pointerId,base:week.map(day=>day.slice()),from:point(cell),on:cell.getAttribute('aria-checked')!=='true'};table.setPointerCapture(event.pointerId);apply(point(cell));};
  table.onpointermove=event=>{if(!painting)return;const cell=document.elementFromPoint(event.clientX,event.clientY)?.closest('#pump-week button');if(cell)apply(point(cell));};
  const finish=()=>{painting=null;};table.onpointerup=finish;table.onpointercancel=finish;table.onlostpointercapture=finish;
  table.onclick=event=>{const cell=event.target.closest('button');if(!cell||busy||event.detail!==0)return;week=paint(week,point(cell),point(cell),cell.getAttribute('aria-checked')!=='true');dirty=true;grid();};
  table.onkeydown=event=>{let row=event.target.closest('button');if(!row)return;let [day,slot]=point(row);if(event.key==='ArrowRight')slot++;else if(event.key==='ArrowLeft')slot--;else if(event.key==='ArrowUp')day--;else if(event.key==='ArrowDown')day++;else return;event.preventDefault();day=Math.max(0,Math.min(6,day));slot=Math.max(0,Math.min(47,slot));focus=day*48+slot;grid();table.querySelectorAll('button')[focus].focus();};
  for(const id of ['pump-upper','pump-lower','pump-schedule-enabled'])$(id).addEventListener('input',()=>{dirty=true;grid();});
  $('pump-settings').onsubmit=event=>{event.preventDefault();const value=fields();if(!validSettings(value)){message('G\u00f3rny pr\u00f3g musi by\u0107 wy\u017cszy od dolnego o co najmniej 0,5\u00b0C.',true);return;}send(value);};
  $('pump-connect-form').onsubmit=event=>{event.preventDefault();send({action:'connect',host:$('pump-host').value.trim(),mac:$('pump-mac').value,username:$('pump-username').value,password:$('pump-password').value});};
  $('pump-scan').onclick=()=>send({action:'scan'});
  $('pump-enabled').onchange=()=>send({action:'feature',enabled:$('pump-enabled').checked});
  $('pump-start').onclick=()=>quickMode(2);
  $('pump-off').onclick=()=>quickMode(0);
  $('pump-auto').onclick=()=>quickMode(1);
  $('pump-reset').onclick=()=>{dirty=false;if(snapshot)render(snapshot);message('');};
  $('pump-device-select').onchange=()=>{const found=snapshot?.devices?.find(d=>d.mac===$('pump-device-select').value);if(found){$('pump-host').value=found.host;$('pump-mac').value=found.mac;}};
  $('pump-host').oninput=()=>{$('pump-mac').value='';};
  for(const [id,direction]of [['pump-earlier',-1],['pump-later',1]])$(id).onclick=()=>$('pump-week-scroll').scrollBy({left:direction*360,behavior:'smooth'});
  grid();icons();
 }
 root.EcoCwuPump={init,show(active){visible=active;if(active)refresh();},refresh};
})(globalThis);
