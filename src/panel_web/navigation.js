(function(){
 function resolveView(hash,allowed){
  let key=typeof hash==='string'&&hash.startsWith('#')?hash.slice(1):'';
  try{key=decodeURIComponent(key);}catch{key='';}
  return allowed.includes(key)?key:'live';
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={resolveView};
 if(typeof window==='undefined')return;
 let views=[],apply=()=>{},current=null;
 function route(next,replace){
  const hash=next==='live'?'':'#'+next;
  if(location.hash===hash)return;
  try{window.history[replace?'replaceState':'pushState'](null,'',location.pathname+location.search+hash);}catch{}
 }
 function restore(){
  const next=resolveView(location.hash,views);route(next,true);
  if(next===current)return;
  current=next;apply(next);
 }
 window.EcoNavigation={
  init(allowed,render){views=[...allowed];apply=render;window.addEventListener('popstate',restore);window.addEventListener('hashchange',restore);restore();},
  remember(next){current=resolveView('#'+next,views);route(current,false);}
 };
})();
