(function(root){
 'use strict';
 const field=(key,name,unit,color,dashed=false)=>({key,name,unit,color,dashed});
 const groups=[
  {id:'temperatures',name:'Kocio\u0142 i CWU',lines:[field('co','Kocio\u0142','\u00b0C','#a13d38'),field('coSet','Zadana kot\u0142a','\u00b0C','#a13d38',true),field('cwu','CWU','\u00b0C','#087e9a'),field('cwuSet','Zadana CWU','\u00b0C','#087e9a',true)]},
  {id:'states',name:'Stany kot\u0142a / Rozpalanie',kind:'states'},
  {id:'combustion',name:'Spaliny i podajnik',lines:[field('exhaust','Spaliny','\u00b0C','#b94d16'),field('feederTemp','Podajnik','\u00b0C','#577b32')]},
  {id:'room',name:'Temperatura pokojowa / ecoSTER 1',lines:[field('room','Pokojowa','\u00b0C','#816393'),field('roomSet','Zadana pokojowa','\u00b0C','#816393',true)]},
  {id:'mixer',name:'Mieszacz 1',lines:[field('mixer','Mieszacz 1','\u00b0C','#197e6c'),field('mixerSet','Zadana mieszacza','\u00b0C','#197e6c',true)]},
  {id:'outside',name:'Temperatura zewn\u0119trzna',lines:[field('outside','Zewn\u0119trzna','\u00b0C','#496dae')]},
  {id:'power',name:'Moc kot\u0142a i wentylator',percent:true,lines:[field('power','Moc kot\u0142a','%','#a13d38'),field('fan','Wentylator','%','#087e9a')]},
  {id:'outputs',name:'Pompy, podajnik i zapalarka',kind:'outputs'},
  {id:'fuel',name:'Poziom pelletu',percent:true,lines:[field('fuel','Poziom pelletu','%','#547b30')]},
  {id:'flow',name:'Strumie\u0144 pelletu',lines:[field('flow','Strumie\u0144 pelletu','kg/h','#547b30')]},
  {id:'kw',name:'Moc cieplna',lines:[field('kw','Moc cieplna','kW','#a13d38')]},
  {id:'oxygen',name:'Tlen / lambda',lines:[field('oxygen','Tlen','%','#3c8190')]}
 ];
 for(const n of [2,3])groups.push({id:'room'+n,name:'ecoSTER '+n,optional:true,lines:[field('room'+n,'Pokojowa '+n,'\u00b0C','#816393'),field('roomSet'+n,'Zadana pokojowa '+n,'\u00b0C','#816393',true)]});
 for(const n of [2,3,4])groups.push({id:'mixer'+n,name:'Mieszacz '+n,optional:true,lines:[field('mixer'+n,'Mieszacz '+n,'\u00b0C','#197e6c'),field('mixerSet'+n,'Zadana mieszacza '+n,'\u00b0C','#197e6c',true)]});
 const outputs=[['pumpCO','Pompa CO'],['pumpCWU','Pompa CWU'],['mixerPump','Pompa mieszacza 1'],['circulation','Cyrkulacja'],['fanOn','Wentylator'],['feeder','Podajnik'],['lighter','Zapalarka'],['alarm','Wyj\u015bcie alarmowe'],['mixerPump2','Pompa mieszacza 2'],['mixerPump3','Pompa mieszacza 3'],['mixerPump4','Pompa mieszacza 4']];
 const phaseColors={'Wy\u0142\u0105czony':'#70777c','Rozpalanie':'#be681a','Stabilizacja':'#9a7840','Praca':'#238265','Nadz\u00f3r':'#5279ac','Post\u00f3j':'#7a6e8b','Wygaszanie':'#a95970','Alarm':'#b63737'};
 const finite=value=>typeof value==='number'&&Number.isFinite(value);
 const localDate=date=>date.getFullYear()+'-'+String(date.getMonth()+1).padStart(2,'0')+'-'+String(date.getDate()).padStart(2,'0');
 function windowFor(value,date,now=Date.now()){
  const end=Math.floor(now/1000),oldest=end-3*86400;
  if(value==='day'){
   if(!/^\d{4}-\d{2}-\d{2}$/.test(date))throw new Error('Wybierz poprawny dzie\u0144');
   const start=new Date(date+'T00:00:00');if(!Number.isFinite(start.getTime())||localDate(start)!==date)throw new Error('Nieprawid\u0142owa data');
   const next=new Date(start);next.setDate(next.getDate()+1);const from=Math.max(Math.floor(start.getTime()/1000),oldest),to=Math.min(Math.floor(next.getTime()/1000)-1,end);
   if(from>to)throw new Error('Dzie\u0144 poza histori\u0105 3 dni');return {from,to};
  }
  const minutes=Number(value);if(![15,30,60,120,360,1440,4320].includes(minutes))throw new Error('Nieprawid\u0142owy zakres historii');return {from:end-minutes*60,to:end};
 }
 function decode(data){
  if(data?.schema!==1||!Array.isArray(data.columns)||!Array.isArray(data.rows)||data.columns.length>64||new Set(data.columns).size!==data.columns.length||!['time','native','mode'].every(k=>data.columns.includes(k)))throw new Error('Nieznany format historii');
  let previous=0;return data.rows.slice(-720).map(row=>{if(!Array.isArray(row)||row.length!==data.columns.length)throw new Error('Niepe\u0142na pr\u00f3bka historii');const point=Object.fromEntries(data.columns.map((key,i)=>[key,finite(row[i])?row[i]:null]));if(!finite(point.time)||point.time<1577836800||point.time<previous||![0,1].includes(point.native))throw new Error('Nieprawid\u0142owy czas lub \u017ar\u00f3d\u0142o historii');previous=point.time;return point;});
 }
 function phase(point,label=root.EcoSettings?.boilerModeLabel){return finite(point.mode)?label(point.mode,point.native?'rs485':'econet'):'Brak odczytu';}
 function segments(points,key,gap=30){const result=[];let current=[];for(const p of points){if(!finite(p[key])){if(current.length)result.push(current);current=[];continue;}if(current.length&&(p.gapBefore||p.time-current.at(-1).time>gap||p.native!==current.at(-1).native)){result.push(current);current=[];}current.push(p);}if(current.length)result.push(current);return result;}
 function intervals(points,end,gap=30){return points.map((p,i)=>({point:p,start:p.time,end:Math.min(points[i+1]?.time??end,p.time+(points[i+1]?.gapBefore?Math.min(gap,60):gap))})).filter(v=>v.end>v.start);}
 function csv(points,label){const lines=groups.flatMap(g=>g.lines||[]),keys=[...new Set(lines.map(f=>f.key))],meta=Object.fromEntries(lines.map(f=>[f.key,f]));return '\uFEFF'+[['Czas','Transport','Stan kot\u0142a',...keys.map(k=>meta[k].name+' ['+meta[k].unit+']'),...outputs.map(o=>o[1])],...points.map(p=>[new Date(p.time*1000).toISOString(),p.native?'RS485':'ecoNET',phase(p,label),...keys.map(k=>finite(p[k])?p[k]:''),...outputs.map(([k])=>finite(p[k])?p[k]:'')])].map(row=>row.map(v=>'"'+String(v).replaceAll('"','""')+'"').join(';')).join('\r\n');}
 const api={groups,outputs,decode,segments,intervals,phase,csv,windowFor,localDate};
 if(typeof module==='object'&&module.exports)module.exports=api;
 if(typeof document==='undefined')return;
 const get=id=>document.getElementById(id),fmt=v=>finite(v)?v.toLocaleString('pl-PL',{maximumFractionDigits:1}):'\u2014',clock=t=>new Date(t*1000).toLocaleTimeString('pl-PL',{hour:'2-digit',minute:'2-digit',second:'2-digit'}),stamp=t=>new Date(t*1000).toLocaleString('pl-PL',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'});
 let points=[],busy=false,loaded=false,lastAttempt=0,error='',hiddenKeys=new Set(),inspectTime=null,lastSnapshot=null,metadata={},loadedKey='';
 const visible=()=>!document.hidden&&!get('history-source').closest('[data-section]').hidden;
 const selected=()=>groups.find(g=>g.id===get('history-source').value)||groups[0];
 const queryKey=()=>get('history-window').value+'|'+get('history-day').value;
 const gap=()=>Math.max(30,metadata.gap_seconds||30);
 function range(){return loadedKey===queryKey()?points:[];}
 function domain(data){const end=data.at(-1)?.time??Date.now()/1000,start=data[0]?.time??end;return [start,end>start?end:start+10];}
 function surface(canvas,height){const width=Math.round(canvas.getBoundingClientRect().width);if(!width)return null;const ratio=Math.min(devicePixelRatio||1,2);canvas.style.height=height+'px';canvas.width=width*ratio;canvas.height=height*ratio;const ctx=canvas.getContext('2d');ctx.scale(ratio,ratio);ctx.fillStyle='#fff';ctx.fillRect(0,0,width,height);ctx.font='11px LatoLocal, sans-serif';return {ctx,width,height};}
 function axes(ctx,width,height,domain,left=50){const [start,end]=domain,x=t=>left+(width-left-12)*(t-start)/(end-start);ctx.fillStyle='#666';ctx.textAlign='center';const ticks=width<450?2:4;for(let i=0;i<=ticks;++i){const t=start+(end-start)*i/ticks;ctx.fillText(end-start>=86400?stamp(t):end-start<180?clock(t):clock(t).slice(0,5),Math.min(width-45,Math.max(45,x(t))),height-7);}ctx.textAlign='left';return x;}
 function numeric(canvas,data,group){const s=surface(canvas,260);if(!s)return;const {ctx,width,height}=s,lines=group.lines.filter(l=>!hiddenKeys.has(l.key)),vals=data.flatMap(p=>lines.map(l=>p[l.key])).filter(finite),bounds=domain(data),x=axes(ctx,width,height,bounds);if(!vals.length){ctx.fillStyle='#6b7175';ctx.fillText(data.length?'Brak odczytu wybranych danych':'Brak pr\u00f3bek',54,35);return;}
  let low=group.percent?0:Math.floor(Math.min(...vals)-1),high=group.percent?Math.max(100,...vals):Math.ceil(Math.max(...vals)+1);if(high<=low)high=low+1;const y=v=>18+(height-50)*(high-v)/(high-low);
  for(let i=0;i<5;++i){const v=high-(high-low)*i/4;ctx.strokeStyle='#e4e7e8';ctx.beginPath();ctx.moveTo(50,y(v));ctx.lineTo(width-12,y(v));ctx.stroke();ctx.fillStyle='#697176';ctx.fillText(fmt(v),5,y(v)+4);}ctx.fillText(group.lines[0].unit,5,12);
  for(const line of lines){ctx.strokeStyle=line.color;ctx.fillStyle=line.color;ctx.lineWidth=1.8;ctx.setLineDash(line.dashed?[5,4]:[]);for(const part of segments(data,line.key,gap())){ctx.beginPath();for(let i=0;i<part.length;++i){const p=part[i];if(!i)ctx.moveTo(x(p.time),y(p[line.key]));else if(line.dashed){ctx.lineTo(x(p.time),y(part[i-1][line.key]));ctx.lineTo(x(p.time),y(p[line.key]));}else ctx.lineTo(x(p.time),y(p[line.key]));}ctx.stroke();if(part.length===1){ctx.beginPath();ctx.arc(x(part[0].time),y(part[0][line.key]),2.5,0,Math.PI*2);ctx.fill();}}}ctx.setLineDash([]);
  if(inspectTime!==null){ctx.strokeStyle='#93999c';ctx.beginPath();ctx.moveTo(x(inspectTime),15);ctx.lineTo(x(inspectTime),height-30);ctx.stroke();}
 }
 function timeline(canvas,data,binary=false){const base=binary?outputs.filter(([key],i)=>i<8||data.some(p=>finite(p[key]))):Object.keys(phaseColors).map(name=>[name,name]);if(!binary)for(const p of data){const name=phase(p);if(name!=='Brak odczytu'&&!base.some(r=>r[0]===name))base.push([name,name]);}
  const s=surface(canvas,base.length*26+34);if(!s)return;const {ctx,width,height}=s,left=width<500?113:150,x=axes(ctx,width,height,domain(data),left);for(let i=0;i<base.length;++i){ctx.fillStyle='#e9edef';ctx.fillRect(left,8+i*26,width-left-12,18);ctx.fillStyle='#4c555a';ctx.fillText(base[i][1],3,21+i*26,left-9);}
  for(const v of intervals(data,domain(data)[1],gap()))for(let i=0;i<base.length;++i){const [key]=base[i],value=binary?v.point[key]:phase(v.point),valid=binary?(value===0||value===1):value!=='Brak odczytu';if(!valid)continue;const active=binary?value===1:value===key;if(active){ctx.fillStyle=binary?(key==='alarm'?'#b63737':'#21836d'):phaseColors[key]||'#647c8b';ctx.fillRect(x(v.start),8+i*26,Math.max(1,x(v.end)-x(v.start)),18);}else if(binary){ctx.fillStyle='#cbd3d7';ctx.fillRect(x(v.start),8+i*26,Math.max(1,x(v.end)-x(v.start)),18);}}
 }
 function element(tag,text,cls=''){const el=document.createElement(tag);el.textContent=text;el.className=cls;return el;}
 function render(){if(!visible())return;const data=range(),group=selected(),archive=metadata.archive||{};get('history-title').textContent=group.name;
  get('history-range').textContent=data.length?stamp(data[0].time)+' - '+stamp(data.at(-1).time)+' / '+data.length+' pr\u00f3bek':'Brak pr\u00f3bek w wybranym zakresie';
  get('history-storage').textContent=archive.loading?'Otwieranie archiwum':archive.ready?'Archiwum 3 dni / zapis 1 min / '+(archive.samples||0)+' pr\u00f3bek'+(archive.oldest_epoch?' / od '+stamp(archive.oldest_epoch):''):'Archiwum niedost\u0119pne / tylko RAM';
  get('history-resolution').textContent=data.length?'Wykres: '+(metadata.resolution_seconds>=60?'oko\u0142o '+Math.ceil(metadata.resolution_seconds/60)+' min':(metadata.resolution_seconds||10)+' s'):'';
  get('history-error').textContent=error||archive.error||(archive.bad_records?'Pomini\u0119te uszkodzone pr\u00f3bki: '+archive.bad_records:archive.dropped?'Niezapisane pr\u00f3bki: '+archive.dropped:archive.newest_epoch>Date.now()/1000+60?'Zegar wcze\u015bniejszy ni\u017c ostatni zapis archiwum':'');get('history-error').hidden=!get('history-error').textContent;
  const today=localDate(new Date()),oldest=localDate(new Date(Date.now()-3*86400000));get('history-day').min=oldest;get('history-day').max=today;get('history-prev').disabled=get('history-day').value<=oldest;get('history-next').disabled=get('history-day').value>=today;
  for(const option of get('history-source').options){const g=groups.find(g=>g.id===option.value),available=!g.lines||g.lines.some(l=>points.some(p=>finite(p[l.key])));option.textContent=g.name+(loaded&&!available?' / brak odczytu':'');option.hidden=!!g.optional&&!available;}
  const legend=get('history-legend'),signature=group.id+'|'+(group.lines||[]).map(l=>Number(hiddenKeys.has(l.key))).join('');if(legend.dataset.signature!==signature){legend.replaceChildren();for(const line of group.lines||[]){const label=element('label',''),check=document.createElement('input'),swatch=element('span','','history-swatch');check.type='checkbox';check.checked=!hiddenKeys.has(line.key);check.dataset.key=line.key;swatch.style.borderColor=line.color;swatch.style.borderTopStyle=line.dashed?'dashed':'solid';check.onchange=()=>{if(check.checked)hiddenKeys.delete(line.key);else hiddenKeys.add(line.key);render();};label.append(check,swatch,element('span',line.name));legend.append(label);}legend.dataset.signature=signature;}
  if(group.kind)timeline(get('chart'),data,group.kind==='outputs');else numeric(get('chart'),data,group);get('history-phases-band').hidden=group.kind==='states';if(group.kind!=='states')timeline(get('history-phases'),data);
  get('history-download').disabled=!data.length;const table=document.createElement('table'),head=document.createElement('thead'),tr=document.createElement('tr'),columns=group.lines||[];for(const name of ['Czas','Stan kot\u0142a',...(group.kind==='outputs'?outputs.map(o=>o[1]):columns.map(l=>l.name+' ['+l.unit+']'))])tr.append(element('th',name));head.append(tr);table.append(head);const body=document.createElement('tbody');for(const p of data.slice(-12).reverse()){const row=document.createElement('tr');for(const value of [stamp(p.time),phase(p),...(group.kind==='outputs'?outputs.map(([key])=>p[key]===1?'ON':p[key]===0?'OFF':'\u2014'):columns.map(l=>fmt(p[l.key])))])row.append(element('td',value));body.append(row);}table.append(body);get('history-table').replaceChildren(table);updateReadout(data,group);
 }
 function updateReadout(data,group){const p=inspectTime===null?data.at(-1):data.reduce((a,b)=>!a||Math.abs(b.time-inspectTime)<Math.abs(a.time-inspectTime)?b:a,null);get('history-readout').textContent=p?[stamp(p.time),phase(p),...(group.lines||[]).filter(l=>!hiddenKeys.has(l.key)).map(l=>l.name+': '+fmt(p[l.key])+' '+l.unit),...(group.kind==='outputs'?outputs.filter(([key])=>finite(p[key])).map(([key,name])=>name+': '+(p[key]?'ON':'OFF')):[])].join(' / '):'Brak pr\u00f3bek';}
 async function refresh(force=false){
  if(!visible()||busy||(!force&&Date.now()-lastAttempt<(get('history-window').value==='day'?60000:10000)))return;
  busy=true;lastAttempt=Date.now();const key=queryKey();get('history-refresh').disabled=true;
  try{const query=windowFor(get('history-window').value,get('history-day').value),data=await request('/api/history?'+new URLSearchParams(query));if(queryKey()===key){points=decode(data);metadata=data;loaded=true;loadedKey=key;error='';}}
  catch(e){if(queryKey()===key)error='Historia: '+e.message+(loadedKey===key&&points.length?' / ostatni poprawny odczyt '+stamp(points.at(-1).time):'');}
  finally{busy=false;get('history-refresh').disabled=false;render();if(queryKey()!==key)refresh(true);}
 }
 function init(){
  for(const group of groups)get('history-source').append(new Option(group.name,group.id));
  get('history-day').value=localDate(new Date());
  get('history-source').onchange=()=>{inspectTime=null;render();};get('history-window').onchange=()=>{inspectTime=null;error='';render();refresh(true);};get('history-refresh').onclick=()=>refresh(true);
  get('history-day').onchange=()=>{get('history-window').value='day';inspectTime=null;error='';render();refresh(true);};
  for(const [id,delta]of [['history-prev',-1],['history-next',1]])get(id).onclick=()=>{const day=new Date(get('history-day').value+'T12:00:00');day.setDate(day.getDate()+delta);get('history-day').value=localDate(day);get('history-day').onchange();};
  get('history-download').onclick=()=>{const url=URL.createObjectURL(new Blob([csv(range())],{type:'text/csv;charset=utf-8'})),a=document.createElement('a');a.href=url;a.download='piec-historia.csv';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
  const inspect=event=>{const group=selected(),rect=event.currentTarget.getBoundingClientRect(),[start,end]=domain(range()),left=group.kind?(rect.width<500?113:150):50;inspectTime=start+Math.max(0,Math.min(1,(event.clientX-rect.left-left)/(rect.width-left-12)))*(end-start);if(!group.kind)numeric(get('chart'),range(),group);updateReadout(range(),group);};
  get('chart').addEventListener('pointermove',inspect);get('chart').addEventListener('pointerdown',inspect);get('chart').addEventListener('pointerleave',()=>{inspectTime=null;render();});window.addEventListener('resize',render);
 }
 root.EcoHistory={...api,init,render,refresh,update(snapshot){if(lastSnapshot!==snapshot){lastSnapshot=snapshot;if(visible()){render();refresh();}}}};
})(globalThis);
