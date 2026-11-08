(function(root){
 'use strict';
 const specs={boiler_ms:{min:1000,max:10000,step:100},panel_ms:{min:500,max:10000,step:100},history_ms:{min:30000,max:600000,step:1000}};
 const defaults={boiler_ms:3000,panel_ms:2000,history_ms:60000};
 function valid(c){return !!c&&Object.entries(specs).every(([key,s])=>Number.isInteger(c[key])&&c[key]>=s.min&&c[key]<=s.max&&c[key]%s.step===0);}
 function milliseconds(value,unit){const n=Number(value)*(unit==='s'?1000:1);return Math.abs(n-Math.round(n))<1e-6?Math.round(n):NaN;}
 if(typeof module!=='undefined'&&module.exports)module.exports={valid,milliseconds,defaults};
 if(typeof document==='undefined')return;
 const el=id=>document.getElementById(id);let saved=null,dirty=false,busy=false,online=false;
 function values(){return Object.fromEntries(Object.keys(specs).map(key=>[key,milliseconds(el('sampling-'+key).value,el('sampling-'+key+'-unit').value)]));}
 function paint(){
  el('sampling-save').disabled=busy||!online||!saved||!dirty||!valid(values());
  el('sampling-cancel').hidden=!dirty;el('sampling-cancel').disabled=busy;
  el('sampling-defaults').disabled=busy||!saved||!online;
  for(const key of Object.keys(specs)){el('sampling-'+key).disabled=busy||!saved||!online;el('sampling-'+key+'-unit').disabled=busy||!saved||!online;}
 }
 function setField(key,ms){
  const input=el('sampling-'+key),factor=el('sampling-'+key+'-unit').value==='s'?1000:1,s=specs[key];
  input.min=s.min/factor;input.max=s.max/factor;input.step=s.step/factor;input.value=ms/factor;
 }
 function fill(c){for(const key of Object.keys(specs))setField(key,c[key]);}
 function changed(){const c=values();dirty=Object.keys(specs).some(key=>c[key]!==saved?.[key]);el('sampling-status').textContent='';paint();}
 function init(){
  for(const key of Object.keys(specs)){
   el('sampling-'+key).oninput=changed;
   const unit=el('sampling-'+key+'-unit');unit.dataset.previous=unit.value;
   unit.onchange=()=>{const ms=milliseconds(el('sampling-'+key).value,unit.dataset.previous);unit.dataset.previous=unit.value;setField(key,ms);changed();};
  }
  el('sampling-defaults').onclick=()=>{fill(defaults);changed();};
  el('sampling-cancel').onclick=()=>{fill(saved);dirty=false;el('sampling-status').textContent='';paint();};
  el('sampling-form').onsubmit=async event=>{
   event.preventDefault();const payload=values();if(busy||!online||!valid(payload))return;busy=true;paint();
   try{const result=await request('/api/sampling',payload);if(!valid(result))throw new Error('Nieprawidłowe potwierdzenie ustawień');saved=result;dirty=false;fill(saved);applyPanelInterval(saved.panel_ms);if(state)state.sampling=saved;el('sampling-status').textContent='Zapisano w ESP. Nowe odstępy już obowiązują.';}
   catch(error){el('sampling-status').textContent=error.message;}
   finally{busy=false;paint();}
  };paint();
 }
 root.EcoSampling={init,render(c,reachable){online=reachable;if(valid(c)&&!busy){saved=c;if(!dirty)fill(saved);if(reachable&&csrf)applyPanelInterval(c.panel_ms);}paint();}};
})(globalThis);
