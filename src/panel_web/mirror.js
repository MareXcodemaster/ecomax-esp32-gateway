'use strict';
(function(root){
 const schedules={boilerTZ:'Kocio\u0142',cwuTZ:'Zbiornik CWU',circPumpTZ:'Pompa cyrkulacyjna',boilerWorkTZ:'Praca kot\u0142a',boilerCleanTZ:'Czyszczenie kot\u0142a',exchangerCleanTZ:'Czyszczenie wymiennika',mixer1TZ:'Mieszacz 1',mixer2TZ:'Mieszacz 2',mixer3TZ:'Mieszacz 3',mixer4TZ:'Mieszacz 4',thermostat1TZ:'Termostat ecoSTER 1'};
 function scheduleHelp(key){
  if(key==='cwuTZ')return {
   description:'Obni\u017ca temperatur\u0119 wody w bojlerze do kran\u00f3w i k\u0105pieli w wybranych godzinach. Nie zmienia temperatury w pokoju.',
   full:'Turkusowe pola: woda w bojlerze ma by\u0107 dogrzewana do zwyk\u0142ej temperatury ustawionej jako \u201eTemperatura zadana CWU\u201d.',
   reduced:'Szare pola: temperatura zadana ni\u017csza o warto\u015b\u0107 \u201eObni\u017c temperatur\u0119 o\u201d. To nie jest ca\u0142kowite wy\u0142\u0105czenie grzania CWU.',
   note:'Przyk\u0142ad: ustawiasz CWU na 45\u00b0C, a obni\u017cenie na 5\u00b0C. Zaznaczasz turkusowo 06:00-09:00 i 17:00-22:00: w tych godzinach celem jest 45\u00b0C, a w pozosta\u0142ych, szarych godzinach 40\u00b0C. Temperatura zmierzona nie zmienia si\u0119 od razu. Zwyk\u0142\u0105 temperatur\u0119 zmienisz w Parametrach u\u017cytkownika > Ustawienia CWU > Temperatura zadana CWU; wielko\u015b\u0107 obni\u017cenia ustawiasz pod tabel\u0105.'
  };
  if(key==='thermostat1TZ')return {
   description:'Prze\u0142\u0105cza temperatur\u0119 pokojow\u0105 mi\u0119dzy komfortow\u0105 i ekonomiczn\u0105. ecoSTER musi pracowa\u0107 w trybie \u201eHarmonogram\u201d. Nie steruje temperatur\u0105 wody w kranach.',
   full:'Turkusowe pola: temperatura komfortowa (dzienna), do kt\u00f3rej ogrzewanie ma d\u0105\u017cy\u0107 w pokoju.',
   reduced:'Szare pola: temperatura ekonomiczna (nocna), do kt\u00f3rej ogrzewanie ma d\u0105\u017cy\u0107 w pokoju. To nie oznacza ca\u0142kowitego wy\u0142\u0105czenia ogrzewania.',
   note:'Przyk\u0142ad: dzienna 22\u00b0C, nocna 20\u00b0C, turkusowe pola od 06:00 do 22:00. Sterownik utrzymuje wtedy cel pokojowy 22\u00b0C w dzie\u0144 i 20\u00b0C w nocy, je\u015bli wybrany jest tryb \u201eHarmonogram\u201d. Obie warto\u015bci zmienisz w Parametrach u\u017cytkownika > ecoSTER T1 > Temperatura zadana dzienna / nocna. Nocna nie jest automatycznie ni\u017csza: zale\u017cy od wpisanej warto\u015bci. Tryby Komfortowy, Ekonomiczny, Party lub Wakacje zast\u0119puj\u0105 plan.'
  };
  if(/^mixer[1-4]TZ$/.test(key))return {
   description:'Obni\u017ca temperatur\u0119 wody p\u0142yn\u0105cej na grzejniki lub pod\u0142og\u00f3wk\u0119 w obiegu mieszacza '+key[5]+'. To nie jest bezpo\u015brednio temperatura pokoju.',
   full:'Turkusowe pola: temperatura zadana obiegu bez obni\u017cenia czasowego.',
   reduced:'Szare pola: temperatura zadana obiegu ni\u017csza o warto\u015b\u0107 \u201eObni\u017c temperatur\u0119 o\u201d.',
   note:'Sterowanie pogodowe i termostat mog\u0105 dodatkowo wp\u0142ywa\u0107 na cel obiegu. Minimalne i maksymalne temperatury ustawione w sterowniku nadal obowi\u0105zuj\u0105.'
  };
  if(key==='boilerTZ')return {
   description:'Obni\u017ca zadan\u0105 temperatur\u0119 wody w samym kotle. Nie okre\u015bla temperatury w pokoju ani w bojlerze CWU.',
   full:'Turkusowe pola: temperatura zadana kot\u0142a bez obni\u017cenia czasowego.',
   reduced:'Szare pola: temperatura zadana kot\u0142a ni\u017csza o warto\u015b\u0107 \u201eObni\u017c temperatur\u0119 o\u201d. Nie oznacza wy\u0142\u0105czenia kot\u0142a.',
   note:'Pozosta\u0142e funkcje i ograniczenia sterownika nadal obowi\u0105zuj\u0105. Nie myl tego planu z osobnym harmonogramem \u201ePraca kot\u0142a\u201d.'
  };
  if(key==='boilerWorkTZ')return {
   description:'Wyznacza godziny dozwolonej pracy kot\u0142a i przerwy. Dozwolona praca nie oznacza ci\u0105g\u0142ego palenia.',
   full:'Turkusowe pola: praca kot\u0142a dozwolona, gdy jest zapotrzebowanie na ciep\u0142o. Nie oznacza ci\u0105g\u0142ego palenia.',
   reduced:'Szare pola: zaplanowana przerwa w pracy kot\u0142a, kt\u00f3ra mo\u017ce te\u017c uniemo\u017cliwi\u0107 zwyk\u0142e grzanie CWU.',
   note:'Ten plan dotyczy pracy kot\u0142a, a nie tylko obni\u017cenia temperatury. Zabezpieczenia sterownika pozostaj\u0105 aktywne.'
  };
  if(key==='circPumpTZ')return {
   description:'Wyznacza godziny dzia\u0142ania obiegu, dzi\u0119ki kt\u00f3remu szybciej masz ciep\u0142\u0105 wod\u0119 w kranie, je\u015bli taka pompa jest zamontowana.',
   full:'Turkusowe pola: cyrkulacja dozwolona wed\u0142ug czas\u00f3w pracy i postoju ustawionych w sterowniku.',
   reduced:'Szare pola: przerwa w cyrkulacji.',
   note:'To nie jest pompa \u0142aduj\u0105ca bojler CWU. Wy\u0142\u0105czenie cyrkulacji nie oznacza wy\u0142\u0105czenia podgrzewania wody w bojlerze.'
  };
  if(key==='boilerCleanTZ'||key==='exchangerCleanTZ')return {
   description:'Wyznacza dozwolone godziny automatycznego czyszczenia '+(key==='boilerCleanTZ'?'kot\u0142a':'wymiennika')+', je\u015bli sterownik obs\u0142uguje dany mechanizm.',
   full:'Turkusowe pola: automatyczne cykle czyszczenia dozwolone.',
   reduced:'Szare pola: przerwa w pracy danego mechanizmu czyszczenia.',
   note:'Plan nie uruchamia czyszczenia co 30 minut. Cz\u0119stotliwo\u015b\u0107 cykli okre\u015blaj\u0105 inne nastawy; funkcja wymaga obs\u0142ugi danego mechanizmu w sterowniku.'
  };
  return {description:'Sterownik udost\u0119pnia ten harmonogram, ale jego szczeg\u00f3\u0142owe dzia\u0142anie nie zosta\u0142o jeszcze opisane.',full:'Znaczenie turkusowych p\u00f3l wymaga potwierdzenia w dokumentacji sterownika.',reduced:'Znaczenie szarych p\u00f3l wymaga potwierdzenia w dokumentacji sterownika.',note:'Nie nale\u017cy zak\u0142ada\u0107, \u017ce ten plan dzia\u0142a tak samo jak harmonogram CWU.'};
 }
 const networkIds=new Set(['172','173','174','175','176','177','178','179','180','181','182','184']);
 function econetNetworkValue(id,data){if(!networkIds.has(String(id)))return undefined;if(!data.econet_network?.fresh)return data.econet_health?.fresh&&!data.econet_health.api_ok?'ecoNET niedost\u0119pny':'Brak aktualnego odczytu ecoNET';return data.econet_network.values?.[id]??null;}
 const fallbackTiles=[['mode'],['boilerPower'],['fuelLevel'],['fanPower'],['tempFlueGas'],['lambdaLevel'],['tempCO','tempCOSet'],['tempCWU','tempCWUSet'],['tempExternalSensor'],['mixerTemp1','mixerSetTemp1'],['tempFeeder'],['ecoSterTemp1','ecoSterSetTemp1']].map((v,i)=>({memberName_:v[0],setMemberName_:v[1],seqNo_:i}));
 const labels={mode:'Stan kot\u0142a',boilerPower:'Moc kot\u0142a',fuelLevel:'Poziom paliwa',fanPower:'Moc wentylatora',tempFlueGas:'Temperatura spalin',lambdaLevel:'Sonda Lambda',tempCO:'Temperatura kot\u0142a',tempCWU:'Temperatura CWU',tempExternalSensor:'Temperatura zewn\u0119trzna',mixerTemp1:'Temperatura mieszacza 1',tempFeeder:'Temperatura podajnika',ecoSterTemp1:'Temperatura ecoSTER 1'};
 function clock(slot){return String(Math.floor(slot/2)).padStart(2,'0')+':'+(slot%2?'30':'00');}
 function decodeSchedule(raw,day){
  if(!Array.isArray(raw)||raw.length!==8||!Number.isInteger(day)||day<0||day>6||!Array.isArray(raw[day])||raw[day].length!==6||raw[day].some(v=>!Number.isInteger(v)||v<0||v>255)||!Array.isArray(raw[7])||raw[7].length<4)return null;
  const slots=raw[day].flatMap(byte=>Array.from({length:8},(_,i)=>!!(byte&(1<<(7-i))))),ranges=[];
  for(let start=0;start<48;){let end=start+1;while(end<48&&slots[end]===slots[start])end++;ranges.push({full:slots[start],start,end,label:clock(start)+'\u2013'+clock(end)});start=end;}
  return {slots,ranges,enabled:raw[7][0]===1,reduction:raw[7][1],min:raw[7][2],max:raw[7][3]};
 }
 function buildTree(structure,names){
  const sections=[],rootNode={name:names[0]||'Menu',entries:[],key:'root',pass:0,locked:false};let cursor=0;
  // Type 7 advances the parent section ordinal, not its menu index.
  structure.forEach((row,position)=>{
   if(row.type===7){if(position>2)cursor++;return;}
   if([0,2,4,5].includes(row.type)){
    if(!sections.length){sections.push(rootNode);return;}
    const parent=sections[cursor]||rootNode,name=names[row.index]||'Grupa '+row.index;
    let node=name===parent.name?parent:parent.entries.find(n=>n.kind==='group'&&n.name===name);
    if(!node){node={kind:'group',name,entries:[],key:parent.key+'/'+row.index,pass:Math.max(parent.pass,row.pass_index||0),locked:parent.locked||!!row.lock};parent.entries.push(node);}
    sections.push(node);
   }else if(row.type===1||row.type===3){const parent=sections[cursor]||rootNode;parent.entries.push({kind:row.type===1?'parameter':'current',index:row.index,data_id:row.data_id,pass:Math.max(parent.pass,row.pass_index||0),locked:parent.locked||!!row.lock});}
  });return rootNode;
 }
 function visibleTree(node,service){
  const result={...node,entries:[]};
  for(const item of node.entries){if(item.pass>1||item.locked)continue;if(item.kind==='group'){const isService=item.name==='Ustawienia serwisowe';if(isService){if(service)result.entries.push(...visibleTree(item,false).entries);}else if(!service){const child=visibleTree(item,false);if(child.entries.length)result.entries.push(child);}}else if(!service)result.entries.push(item);}
  return result;
 }
 function reconcileDraft(draft,current,valid){
  if(!valid)return {...draft,value:draft.dirty?draft.value:'',changed:false};
  if(!draft.dirty)return {value:String(current),baseline:current,dirty:false,changed:false};
  return {...draft,changed:current!==draft.baseline};
 }
 function targetIdForTile(key,data){const native=data.write_transport==='rs485',room={1:'STER_TEMP_NIGHT_1',2:'STER_TEMP_DAY_1',5:(native?'':'legacy:')+'STER_TEMP_SET_PARTY_1',6:(native?'':'legacy:')+'STER_TEMP_SET_SUMMER_1',7:'legacy:STER_TEMP_ANTIFREEZ_1'},mode=Number.isInteger(data.curr?.ecoSterMode1)?data.curr.ecoSterMode1&127:null;return {tempCO:'1280',tempCWU:'1281',mixerTemp1:'1287',ecoSterTemp1:room[mode]||'legacy:STER_TEMP_DAY_1'}[key]||null;}
 function writeIdForEntry(item){return item?.write_enabled&&item?.write_id?item.write_id:null;}
 function isThermostatMenu(item){return item.source==='editParams'||item.source==='RS485_THERMOSTAT_PARAMETERS_220';}
 function parameterReading(item,data,fresh){
  if(!fresh||!data.targets_fresh)return 'Brak aktualnego odczytu';
  if(item?.read_available===false)return item.read_status||'Brak odczytu RS485';
  const value=item?.metadata?.value;
  if(value===null||value===undefined)return 'Nieudost\u0119pniony';
  return value;
 }
 function ticketMatches(data,ticket,fresh,now=Date.now()){
  const live=data.targets?.[ticket.id],equal=(a,b)=>typeof a==='number'&&typeof b==='number'&&Math.abs(a-b)<0.0001;
  return !!(fresh&&data.targets_fresh&&live&&live.write_ready!==false&&['before','min','max','step'].every(k=>equal(live[k],ticket[k]))&&(!(ticket.legacy||ticket.generic&&ticket.profile_mode!==undefined)||equal(live.profile_mode,ticket.profile_mode))&&(!(ticket.legacy&&ticket.transport==='rs485')||['wire_index','parameter_count','profile_raw'].every(k=>equal(live[k],ticket[k])))&&(!(ticket.legacy&&ticket.transport==='rs485'&&ticket.active_profile===false)||equal(live.active_target,ticket.active_target))&&(!ticket.generic||['schema','cached','profile_raw','wire_key','observed_key'].every(k=>JSON.stringify(live[k])===JSON.stringify(ticket[k])))&&data.write_transport===ticket.transport&&ticket.expires*1000>now);
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={decodeSchedule,buildTree,visibleTree,reconcileDraft,clock,targetIdForTile,writeIdForEntry,ticketMatches,isThermostatMenu,parameterReading,econetNetworkValue,scheduleHelp};
 let snapshot=null,isFresh=false,tempId=null,temperatureDraft=null,parameterSelection=null,treeSignature='',trees={};
 const get=id=>document.getElementById(id);
 const make=(tag,cls,value)=>{const el=document.createElement(tag);if(cls)el.className=cls;if(value!==undefined)el.textContent=value;return el;};
 const icon=(name,label)=>{const button=make('button','icon');button.type='button';button.title=label;button.setAttribute('aria-label',label);const i=make('i');i.dataset.lucide=name;button.append(i);return button;};
 const num=(v,d=1)=>typeof v==='number'&&Number.isFinite(v)?v.toLocaleString('pl-PL',{minimumFractionDigits:d,maximumFractionDigits:d}):'---';
 const degrees=v=>num(v)+' \u00b0C';
 function display(value,unit='',enumValue=null){if(value===null||value===undefined)return '---';return enumValue??(typeof value==='boolean'?(value?'Tak':'Nie'):typeof value==='number'?num(value,Number.isInteger(value)?0:1):String(value))+(unit?' '+unit:'');}
 function details(id,rows){const dl=get(id);dl.replaceChildren();for(const [name,value]of rows){const div=make('div');div.append(make('dt','',name),make('dd','',value));dl.append(div);}}
 function systemFresh(){return isFresh&&typeof snapshot.system_age_seconds==='number'&&snapshot.system_age_seconds<45;}
 function rowItem(entry){return entry.kind==='current'?null:snapshot.menu.find(m=>String(m.index)===String(entry.index));}
 function entryName(entry){if(entry.kind==='current')return snapshot.names[entry.data_id]?.name||'RAW / ID '+entry.data_id;return rowItem(entry)?.name||'Parametr '+entry.index;}
 function entryValue(entry){
  if(entry.kind!=='current'){const item=rowItem(entry),value=parameterReading(item,snapshot,isFresh);return typeof value==='string'?value:display(value,item?.unit_name,item?.enum_text)+(item?.cached?' (pami\u0119\u0107 ecoNET)':'');}
  const meta=snapshot.names[entry.data_id],network=econetNetworkValue(entry.data_id,snapshot),value=network!==undefined?network:isFresh?snapshot.values[entry.data_id]:null,unit=meta?.unit;
  if(network!==undefined&&!snapshot.econet_network?.fresh)return network;
  const definition=unit>=31?snapshot.enums[meta.special]:null,position=Number(value)-definition?.first;
  return display(value,unit>=0&&unit<snapshot.units.length?snapshot.units[unit]:'',definition&&Number.isInteger(position)?definition.values[position]??null:null);
 }
 function openParameter(entry){
  parameterSelection={entry};updateParameter();get('parameter-dialog').showModal();
 }
 function updateParameter(){
  if(!parameterSelection)return;if(parameterSelection.key){updateCurrent(parameterSelection.key);return;}
  const entry=parameterSelection.entry;
  const item=rowItem(entry);get('parameter-name').textContent=entryName(entry);
  details('parameter-values',[['Warto\u015b\u0107',entryValue(entry)],['Dost\u0119p','Tylko odczyt'],['Indeks / ID',String(entry.kind==='current'?entry.data_id:entry.index)],...(item?[['Zakres',display(item.metadata.minv)+' \u2013 '+display(item.metadata.maxv)],['Grupa',item.category||'---']]:[])]);
  get('parameter-description').textContent=item?.description||'';get('parameter-raw').textContent=JSON.stringify(item||{...entry,metadata:snapshot.names[entry.data_id]},null,2);
 }
 function openTemperature(id){
  if(/^(rm:|legacy:)/.test(id)){EcoSettings.open(id);return;}
  const target=snapshot.targets?.[id];if(!isFresh||!snapshot.targets_fresh||!target){notice('Brak aktualnego, sp\u00f3jnego odczytu nastawy.',true);return;}if(target.write_ready===false){notice(target.write_error||'Zapis tej nastawy jest zablokowany',true);return;}
  tempId=id;temperatureDraft={value:String(target.before),baseline:target.before,dirty:false};get('temperature-name').textContent=target.name;
  get('temperature-profile-field').hidden=!target.legacy;if(target.legacy)get('temperature-profile').value=id;
  get('temperature-error').textContent='';updateTemperature();get('temperature-dialog').showModal();
 }
 function updateTemperature(){
  if(!tempId||!temperatureDraft)return;const target=snapshot.targets?.[tempId],valid=isFresh&&snapshot.targets_fresh&&!!target;
  get('temperature-setting').textContent='Obecna nastawa: '+degrees(valid?target.before:null);
  get('temperature-measured').textContent='Temperatura zmierzona: '+degrees(isFresh?snapshot.curr[target?.measured_key]:null);
  get('temperature-effective').textContent=valid?(target.write_ready===false?target.write_error:targetSummary(target)):'Brak aktualnego odczytu nastawy';
  temperatureDraft=reconcileDraft(temperatureDraft,target?.before,valid);const input=get('temperature-value');if(input.value!==temperatureDraft.value)input.value=temperatureDraft.value;
  if(valid){input.min=target.min;input.max=target.max;input.step=target.step;get('temperature-limits').textContent='Zakres: '+num(target.min,0)+'\u2013'+num(target.max,0)+' \u00b0C / krok: '+num(target.step)+' \u00b0C';}
  for(const option of get('temperature-profile').options)option.disabled=!snapshot.targets?.[option.value];
  get('temperature-update').textContent=temperatureDraft.changed?'Nastawa zmieni\u0142a si\u0119 poza formularzem: '+degrees(temperatureDraft.baseline)+' \u2192 '+degrees(target.before)+'. Wpisana nowa warto\u015b\u0107 nie zosta\u0142a nadpisana.':'';
  get('temperature-form').querySelector('[type=submit]').disabled=!valid||target.write_ready===false||actionBusy||snapshot.busy||savingTemperature;
 }
 function updateDialogs(){
  if(get('parameter-dialog').open)updateParameter();if(get('temperature-dialog').open)updateTemperature();
 }
 function renderTiles(){
  const container=get('mirror-tiles'),tiles=(snapshot.tiles.length?snapshot.tiles:fallbackTiles).slice().sort((a,b)=>a.seqNo_-b.seqNo_),signature=JSON.stringify(tiles);
  if(container.dataset.signature!==signature){container.replaceChildren();for(const tile of tiles){const key=tile.memberName_;if(!key)continue;const article=make('article','eco-tile');article.dataset.member=key;
   const heading=make('div','tile-title',labels[key]||currentName(key));article.append(heading);const body=make('div','tile-body');
   if(tile.setMemberName_){body.append(reading('set',true),reading('actual',false));const id=targetIdForTile(key,snapshot);if(id&&!key.startsWith('ecoSter')){body.querySelector('[data-reading=set] small').textContent='temp. ustawiona [\u00b0C]';body.querySelector('[data-reading=set]>div').append(make('small','tile-target-now',''));}const button=icon(id?'pencil':'info',id?'Zmie\u0144 temperatur\u0119 ustawion\u0105: '+labels[key]:'Szczeg\u00f3\u0142y: '+(labels[key]||key));button.classList.add('tile-action');button.onclick=()=>id?openTemperature(targetIdForTile(key,snapshot)):openCurrent(key);article.append(button);}
   else if(key.startsWith('temp'))body.append(reading('actual',false));
   else{const graphic=make('div','tile-symbol'),i=make('i');i.dataset.lucide=({mode:'power',boilerPower:'flame',lambdaLevel:'activity',fanPower:'fan',fuelLevel:'wheat'})[key]||'gauge';graphic.append(i);body.append(graphic,make('div',key==='mode'||key==='lambdaLevel'?'tile-mode':'tile-value'));}
   article.append(body);container.append(article);
  }container.dataset.signature=signature;icons();}
  const curr=isFresh?snapshot.curr:{};
  for(const [i,tile]of tiles.entries()){const el=container.children[i];if(!el)continue;const key=tile.memberName_;el.dataset.stale=String(!isFresh);const actual=el.querySelector('[data-reading=actual] .tile-value');
   if(actual)actual.textContent=num(curr[key]);const set=el.querySelector('[data-reading=set] .tile-value');if(set){const id=targetIdForTile(key,snapshot),target=id?snapshot.targets?.[id]:null;set.textContent=num(id&&!key.startsWith('ecoSter')?(isFresh&&snapshot.targets_fresh?target?.before:null):curr[tile.setMemberName_]);const active=el.querySelector('.tile-target-now');if(active)active.textContent='Cel teraz: '+degrees(curr[tile.setMemberName_]);const button=el.querySelector('.tile-action');if(button&&id){button.disabled=!isFresh||!snapshot.targets_fresh||!target||target.write_ready===false||actionBusy||snapshot.busy;button.title=target?.write_ready===false?target.write_error||'Zapis zablokowany':'Zmie\u0144 temperatur\u0119 ustawion\u0105: '+labels[key];}}
   if(!actual){const value=el.querySelector('.tile-value,.tile-mode');value.textContent=key==='mode'?(isFresh?EcoSettings.boilerModeLabel(curr.mode,snapshot.write_transport):'---'):key==='lambdaLevel'?(isFresh?(curr.lambdaStatus===0?'STOP':num(curr[key]/10)):'---'):num(curr[key],0)+' %';}
  }
 }
 function reading(kind,set){const row=make('div','tile-reading '+(set?'set':''));row.dataset.reading=kind;const graphic=make('div','tile-symbol'),i=make('i');i.dataset.lucide=set?'thermometer-sun':'thermometer';graphic.append(i);const content=make('div');content.append(make('div','tile-value','---'),make('small','',set?'temp. zadana [\u00b0C]':'temp. aktualna [\u00b0C]'));row.append(graphic,content);return row;}
 function openCurrent(key){parameterSelection={key};updateCurrent(key);get('parameter-dialog').showModal();}
 function updateCurrent(key){get('parameter-name').textContent=labels[key]||currentName(key);details('parameter-values',[['Temperatura aktualna',degrees(isFresh?snapshot.curr[key]:null)],['Temperatura zadana',degrees(isFresh?snapshot.curr[{mixerTemp1:'mixerSetTemp1',ecoSterTemp1:'ecoSterSetTemp1'}[key]]:null)],['Dost\u0119p','Tylko odczyt']]);get('parameter-description').textContent='';get('parameter-raw').textContent='';}
 function updateTrees(){
  const signature=JSON.stringify([snapshot.structure,snapshot.category_names,snapshot.menu.filter(m=>isThermostatMenu(m)||m.write_id).map(m=>[m.index,m.write_id,m.write_enabled])]);if(treeSignature===signature)return;treeSignature=signature;
  const all=buildTree(snapshot.structure,snapshot.category_names);trees={user:visibleTree(all,false),service:visibleTree(all,true)};
  for(const item of snapshot.menu.filter(isThermostatMenu)){let group=trees.user.entries.find(g=>g.name===item.category);if(!group){group={kind:'group',name:item.category,key:'ster/'+item.category,entries:[]};trees.user.entries.push(group);}group.entries.push({kind:'parameter',index:item.index});}
  get('user-tree').dataset.signature='';get('service-tree').dataset.signature='';
 }
 function renderTree(kind){
  updateTrees();const query=get(kind+'-search').value.trim().toLocaleLowerCase('pl'),container=get(kind+'-tree'),signature=treeSignature+'|'+query;
  if(container.dataset.signature!==signature){const open=new Set([...container.querySelectorAll('details[open]')].map(el=>el.dataset.key));container.replaceChildren();
   function append(entries,parent){for(const entry of entries){if(entry.kind==='group'){const visible=matches(entry,query);if(!visible)continue;const group=make('details','param-group');group.dataset.key=entry.key;group.open=!!query||open.has(entry.key);group.append(make('summary','',entry.name));const content=make('div','group-content');append(entry.entries,content);group.append(content);parent.append(group);}else{if(query&&!entryName(entry).toLocaleLowerCase('pl').includes(query))continue;const row=make('div','parameter-row');row.dataset.kind=entry.kind;row.dataset.index=entry.index;row.dataset.dataId=entry.data_id??'';row.append(make('span','param-name',entryName(entry)),make('strong','param-value',entryValue(entry)));const id=entry.kind==='parameter'?writeIdForEntry(rowItem(entry)):null;const button=icon(id?'pencil':'info',id?'Zmie\u0144: '+entryName(entry):'Szczeg\u00f3\u0142y: '+entryName(entry));if(id)button.dataset.writeId=id;button.onclick=()=>id?openTemperature(id):openParameter(entry);row.append(button);parent.append(row);}}}
   append(trees[kind]?.entries||[],container);if(!container.children.length)container.append(make('p','muted','Brak parametr\u00f3w'));container.dataset.signature=signature;icons();
  }
  for(const row of container.querySelectorAll('.parameter-row')){const entry={kind:row.dataset.kind,index:row.dataset.index,data_id:row.dataset.dataId};row.querySelector('.param-value').textContent=entryValue(entry);row.dataset.stale=String(!isFresh);const button=row.querySelector('[data-write-id]');if(button)button.disabled=!isFresh||!snapshot.targets_fresh||!snapshot.targets?.[button.dataset.writeId]||actionBusy||snapshot.busy;}
 }
 function matches(entry,query){return !query||entry.name?.toLocaleLowerCase('pl').includes(query)||entry.entries?.some(child=>child.kind==='group'?matches(child,query):entryName(child).toLocaleLowerCase('pl').includes(query));}
 function renderSchedule(){
  const source=get('schedule-source'),raw=EcoSettings.scheduleData(snapshot),keys=Object.keys(schedules).filter(key=>raw[key]);for(const key of Object.keys(raw))if(!keys.includes(key))keys.push(key);
  const signature=JSON.stringify(keys);if(source.dataset.signature!==signature){const previous=source.value;source.replaceChildren(...keys.map(key=>new Option(schedules[key]||key,key)));source.value=keys.includes(previous)?previous:keys.includes('cwuTZ')?'cwuTZ':keys[0]||'';source.dataset.signature=signature;}
  const help=scheduleHelp(source.value);for(const [id,text]of [['schedule-description',help.description],['schedule-help-full',help.full],['schedule-help-reduced',help.reduced],['schedule-help-note',help.note]])get(id).textContent=text;
  const room=source.value==='thermostat1TZ',decoded=EcoSettings.scheduleFresh(snapshot,isFresh,source.value)?decodeSchedule(raw[source.value],0):null,noReduction=room||['circPumpTZ','boilerWorkTZ','boilerCleanTZ','exchangerCleanTZ','embeddedThermo'].includes(source.value),fullLabel=room?'Komfortowa':noReduction?'Praca dozwolona':'Bez obni\u017cenia',reducedLabel=room?'Ekonomiczna':noReduction?'Przerwa':'Obni\u017cona temperatura';
  get('schedule-enabled').checked=!!decoded?.enabled;get('schedule-enabled').closest('label').hidden=!room&&!!decoded&&decoded.min===decoded.max;get('schedule-reduction').parentElement.hidden=noReduction||!!decoded&&decoded.min>=decoded.max;
  const legend=document.querySelector('.schedule-legend');legend.replaceChildren();for(const [cls,label]of [['full',fullLabel],['reduced',reducedLabel]]){const span=make('span');span.append(make('i',cls),make('span','',label));legend.append(span);}
  const native=snapshot.write_transport==='rs485',readEpoch=native?(room?snapshot.thermostat_schedule_status:snapshot.schedule_status)?.read_epoch:snapshot.system_read_epoch;
  get('schedule-updated').textContent=(readEpoch?(native?(room?'ecoSTER / RS485':'RS485'):'ecoNET')+' / odczyt: '+new Date(readEpoch*1000).toLocaleTimeString('pl-PL'):'')+(room?' / Tryb ecoSTER: '+(EcoSettings.roomModes[isFresh?EcoSettings.roomMode(snapshot.curr?.ecoSterMode1):null]||'Brak odczytu'):'');
  EcoSettings.schedule(snapshot,isFresh,{full:fullLabel,reduced:reducedLabel});
 }
 function renderAlarms(){
  const list=get('alarm-list');list.replaceChildren();const native=snapshot.write_transport==='rs485',status=snapshot.alarm_status||{};
  if(native){
   list.append(make('h3','',!isFresh||!status.count_fresh?'Brak aktualnego licznika powiadomie\u0144':status.pending_count===0?'Brak bie\u017c\u0105cych powiadomie\u0144':'Powiadomienia kot\u0142a: '+status.pending_count));
   if(!status.history_fresh||!Array.isArray(snapshot.alarms)){list.append(make('p','alarm-empty',status.error||'Brak aktualnych szczeg\u00f3\u0142\u00f3w alarm\u00f3w'));return;}
   list.append(make('p','muted','Historia: '+status.returned_count+' / '+status.total_count+' zg\u0142osze\u0144'+(status.read_epoch?' / '+new Date(status.read_epoch*1000).toLocaleTimeString('pl-PL'):'')));
   if(status.error)list.append(make('p','muted',status.error));
   for(const alarm of snapshot.alarms){const el=make('div','alarm-row');el.append(make('strong','',snapshot.alarm_names?.[alarm.code]||'Alarm '+alarm.code),make('p','',alarm.active?'Nie zamkni\u0119ty przez sterownik':'Zako\u0144czony'),make('p','muted',(alarm.from||'Nieznany czas').replace('T',' ')+' / '+(alarm.to||'---').replace('T',' ')));list.append(el);}
   return;
  }
  if(!systemFresh()||!Array.isArray(snapshot.alarms)){list.append(make('p','alarm-empty','Brak aktualnego odczytu alarm\u00f3w'));return;}
  if(!snapshot.alarms.length){list.append(make('h3','alarm-empty','Brak aktualnych alarm\u00f3w'));return;}
  for(const alarm of snapshot.alarms){const el=make('div','alarm-row'),index=typeof alarm==='number'?alarm:alarm.index??alarm.id;el.append(make('strong','',snapshot.alarm_names[index]||alarm.name||'Alarm '+(index??'')),make('pre','',JSON.stringify(alarm,null,2)));list.append(el);}
 }
 function renderHistory(){EcoHistory.update(snapshot);}
 function renderSettings(){const container=get('settings-values');container.replaceChildren();for(const [name,value]of [['Sterownik',snapshot.system.controllerID],['Oprogramowanie ecoNET',snapshot.system.softVer],['Oprogramowanie modu\u0142u A',snapshot.system.moduleASoftVer],['Oprogramowanie panelu',snapshot.system.modulePanelSoftVer],['Adres lokalny ecoNET',snapshot.host],['Identyfikator schematu',snapshot.system.schemaID],['Wersja panelu lokalnego',snapshot.version]]){const row=make('div','parameter-row');row.append(make('span','param-name',name),make('strong','param-value',value??'---'));container.append(row);}}
 function renderEspWifi(){const network=snapshot.esp_network||{},container=get('esp-wifi-values');container.replaceChildren();for(const [name,value]of [['Urz\u0105dzenie','ESP32-S3'],['Stan Wi-Fi',network.connected?'Po\u0142\u0105czony':'Roz\u0142\u0105czony'],['SSID',network.ssid],['Adres IP',network.connected?network.ip:null],['Si\u0142a sygna\u0142u',network.connected&&Number.isFinite(network.rssi_dbm)?network.rssi_dbm+' dBm':null],['Kana\u0142',network.connected?network.channel:null],['Adres mDNS',network.connected&&network.mdns?'piec.local':null],['Wersja firmware',snapshot.version]]){const row=make('div','parameter-row');row.append(make('span','param-name',name),make('strong','param-value',value??'---'));container.append(row);}}
 function renderSchema(){
  const c=isFresh?snapshot.curr:{},graphic=get('schema-graphic');graphic.hidden=false;graphic.setAttribute('role','group');graphic.setAttribute('aria-label','Podgl\u0105d odczyt\u00f3w instalacji');graphic.replaceChildren();
  const layout=make('div','installation-grid');
  for(const [name,key,target,pump,symbol]of [['Kocio\u0142','tempCO','tempCOSet',null,'flame'],['Woda do kran\u00f3w / CWU','tempCWU','tempCWUSet','pumpCWUWorks','droplets'],['Obieg mieszacza 1','mixerTemp1','mixerSetTemp1','mixerPumpWorks1','route'],['Pok\u00f3j / ecoSTER','ecoSterTemp1','ecoSterSetTemp1',null,'house']]){
   const block=make('div','installation-node'),heading=make('h3'),i=make('i');i.dataset.lucide=symbol;heading.append(i,document.createTextNode(name));block.append(heading);
   for(const [label,value]of [['Zmierzona',degrees(c[key])],['Zadana teraz',degrees(c[target])],...(pump?[['Pompa',typeof c[pump]==='boolean'?(c[pump]?'Pracuje':'Post\u00f3j'):'Brak odczytu']]:[])]){const row=make('div','installation-reading');row.append(make('span','',label),make('strong','',value));block.append(row);}layout.append(block);
  }
  graphic.append(layout);const container=get('schema-readings');container.replaceChildren();
  for(const [label,key,unit]of [['Spaliny','tempFlueGas','\u00b0C'],['Na zewn\u0105trz','tempExternalSensor','\u00b0C'],['Strumie\u0144 paliwa','fuelStream','kg/h'],['Poziom paliwa','fuelLevel','%']]){const row=make('div');row.append(make('span','',label),make('strong','',display(c[key],unit)));container.append(row);}icons();
 }
 function init(){
  document.body.dataset.view='live';const syncNav=()=>get('nav-toggle').setAttribute('aria-expanded',String(matchMedia('(max-width: 900px)').matches?document.body.classList.contains('nav-open'):!document.body.classList.contains('nav-closed')));syncNav();window.addEventListener('resize',syncNav);get('nav-toggle').onclick=()=>{const mobile=matchMedia('(max-width: 900px)').matches;document.body.classList.toggle(mobile?'nav-open':'nav-closed');syncNav();if(view==='history')drawChart();};
  for(const kind of ['user','service'])get(kind+'-search').addEventListener('input',()=>renderTree(kind));
  get('schedule-source').onchange=renderSchedule;EcoHistory.init();
  get('parameter-close').onclick=()=>get('parameter-dialog').close();
  for(const input of document.querySelectorAll('.write-form input[type=number]'))input.addEventListener('input',()=>{input.dataset.dirty='true';input.dataset.draft=input.value;});
  get('temperature-value').addEventListener('input',()=>{if(temperatureDraft){temperatureDraft.value=get('temperature-value').value;temperatureDraft.dirty=temperatureDraft.value!==String(temperatureDraft.baseline);updateTemperature();}});
  get('temperature-profile').onchange=()=>{const id=get('temperature-profile').value;get('temperature-dialog').close();openTemperature(id);};
  const cancel=()=>{tempId=null;get('temperature-dialog').close();};get('temperature-close').onclick=cancel;get('temperature-cancel').onclick=cancel;get('temperature-dialog').addEventListener('cancel',()=>{tempId=null;});
  get('temperature-form').onsubmit=async event=>{event.preventDefault();if(!tempId)return;const id=tempId,button=event.submitter;button.disabled=true;try{await saveTemperature(id,Number(get('temperature-value').value),()=>get('temperature-dialog').close());tempId=null;}catch(error){if(get('temperature-dialog').open)get('temperature-error').textContent=error.message;else notice(error.message,true);}finally{if(get('temperature-dialog').open)updateTemperature();}};
  EcoSettings.init();
  EcoPellet.init();
 }
 function render(view,data,fresh){snapshot=data;isFresh=fresh;EcoSettings.render(data,fresh);updateDialogs();switch(view){case'pellet':EcoPellet.render(data.pellet,fresh);break;case'live':renderTiles();break;case'user':case'service':renderTree(view);break;case'schema':renderSchema();break;case'schedule':renderSchedule();break;case'alarms':renderAlarms();break;case'history':renderHistory();break;case'settings':renderSettings();break;case'espwifi':renderEspWifi();break;case'tests':for(const form of document.querySelectorAll('.write-form')){const input=form.querySelector('input[type=number]'),target=data.targets?.[form.dataset.writeId];input.value=input.dataset.dirty==='true'?input.dataset.draft:isFresh&&data.targets_fresh&&target?target.before:'';}break;}}
 root.EcoMirror={init,render,openTemperature,ticketMatches,decodeSchedule,clock};
})(globalThis);
