'use strict';
(function(root){
 const key='piec.tile-order.v1';
 function arrange(tiles,saved=[]){
  const unique=new Map();
  for(const tile of [...tiles].sort((a,b)=>(a.seqNo_||0)-(b.seqNo_||0)))if(typeof tile.memberName_==='string'&&tile.memberName_&&!unique.has(tile.memberName_))unique.set(tile.memberName_,tile);
  const result=[];
  for(const name of Array.isArray(saved)?saved.slice(0,128):[])if(unique.has(name)){result.push(unique.get(name));unique.delete(name);}
  return result.concat([...unique.values()]);
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={arrange};
 if(typeof document==='undefined')return;
 let saved=[],tiles=[],container,editing=false,dragging=false;
 try{saved=JSON.parse(localStorage.getItem(key)||'[]');}catch{}
 function order(input){tiles=input;return arrange(input,saved);}
 function persist(){
  saved=[...container.children].map(el=>el.dataset.member);
  try{localStorage.setItem(key,JSON.stringify(saved));document.getElementById('tile-layout-status').textContent='Uk\u0142ad zapisany.';}
  catch{document.getElementById('tile-layout-status').textContent='Uk\u0142ad zachowany do zamkni\u0119cia strony. Pami\u0119\u0107 przegl\u0105darki jest niedost\u0119pna.';}
  container.dataset.signature=JSON.stringify(arrange(tiles,saved));
 }
 function decorate(){
  for(const tile of container.children){
   if(tile.querySelector('.tile-grip'))continue;
   const button=document.createElement('button');button.type='button';button.className='icon tile-grip';button.hidden=!editing;
   const name=tile.querySelector('.tile-title').textContent;button.title='Przesu\u0144: '+name;button.setAttribute('aria-label',button.title);
   const icon=document.createElement('i');icon.dataset.lucide='grip';button.append(icon);
   button.onkeydown=event=>{
    if(!editing||!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(event.key))return;
    event.preventDefault();const all=[...container.children],index=all.indexOf(tile),columns=getComputedStyle(container).gridTemplateColumns.split(' ').length;
    const delta={ArrowLeft:-1,ArrowRight:1,ArrowUp:-columns,ArrowDown:columns}[event.key],target=all[Math.max(0,Math.min(all.length-1,index+delta))];
    if(target===tile)return;
    if(delta<0)container.insertBefore(tile,target);else container.insertBefore(tile,target.nextSibling);
    persist();button.focus();
   };
   tile.append(button);
  }
 }
 function init(){
  container=document.getElementById('mirror-tiles');
  const toggle=document.getElementById('tile-layout-edit'),reset=document.getElementById('tile-layout-reset');
  const sorter=new Sortable(container,{animation:120,handle:'.tile-grip',disabled:true,delay:180,delayOnTouchOnly:true,touchStartThreshold:4,ghostClass:'tile-ghost',onStart:()=>{dragging=true;},onEnd:()=>{dragging=false;persist();}});
  toggle.onclick=()=>{
   editing=!editing;container.classList.toggle('editing-layout',editing);toggle.setAttribute('aria-pressed',String(editing));reset.hidden=!editing;sorter.option('disabled',!editing);
   document.getElementById('tile-layout-edit-label').textContent=editing?'Gotowe':'Kafelki';
   toggle.title=editing?'Zako\u0144cz edycj\u0119 kafelk\u00f3w':'Edytuj uk\u0142ad kafelk\u00f3w';toggle.setAttribute('aria-label',toggle.title);
   for(const handle of container.querySelectorAll('.tile-grip'))handle.hidden=!editing;
   document.getElementById('tile-layout-status').textContent='';
  };
  reset.onclick=()=>{
   saved=[];try{localStorage.removeItem(key);}catch{}
   const elements=new Map([...container.children].map(el=>[el.dataset.member,el]));
   for(const tile of arrange(tiles))if(elements.has(tile.memberName_))container.append(elements.get(tile.memberName_));
   container.dataset.signature=JSON.stringify(arrange(tiles));document.getElementById('tile-layout-status').textContent='Przywr\u00f3cono domy\u015blny uk\u0142ad.';
  };
 }
 root.EcoTileLayout={init,order,decorate,isDragging:()=>dragging};
})(globalThis);
