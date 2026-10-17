'use strict';
(function(){
 function init(){
  const menu=document.getElementById('panel-tools'),summary=menu.querySelector('summary');
  const actions=()=>[...menu.querySelectorAll('button,a[href]')].filter(el=>!el.disabled&&!el.hidden&&el.getClientRects().length);
  document.addEventListener('pointerdown',event=>{if(menu.open&&!menu.contains(event.target))menu.open=false;});
  menu.addEventListener('focusout',event=>{if(event.relatedTarget&&!menu.contains(event.relatedTarget))menu.open=false;});
  menu.addEventListener('click',event=>{if(event.target.closest('.tools-list button,.tools-list a'))menu.open=false;});
  menu.addEventListener('keydown',event=>{
   if(event.key==='Escape'&&menu.open){event.preventDefault();menu.open=false;summary.focus();return;}
   if(!['ArrowDown','ArrowUp','Home','End'].includes(event.key))return;
   event.preventDefault();menu.open=true;
   const items=actions();if(!items.length)return;
   const index=items.indexOf(document.activeElement);
   const next=event.key==='Home'?0:event.key==='End'?items.length-1:index<0?(event.key==='ArrowUp'?items.length-1:0):(index+(event.key==='ArrowUp'?-1:1)+items.length)%items.length;
   items[next].focus();
  });
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',init,{once:true});else init();
})();
