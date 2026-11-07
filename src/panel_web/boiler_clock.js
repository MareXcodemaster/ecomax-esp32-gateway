(function(root){
 'use strict';
 function reading(data,online,elapsed=0){
  const age=Number(data?.age_ms)+Math.max(0,elapsed);
  const valid=online&&data?.fresh&&Number.isFinite(age)&&age<10000&&/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(data?.device_time||'');
  const stamp=valid?Date.parse(data.device_time.replace(' ','T')+'Z')+age:NaN;
  if(!Number.isFinite(stamp))return {time:'—',date:'Brak aktualnego odczytu',fresh:false};
  const text=new Date(stamp).toISOString();return {time:text.slice(11,19),date:text.slice(8,10)+'.'+text.slice(5,7)+'.'+text.slice(0,4),fresh:true};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={reading};
 if(typeof document==='undefined')return;
 const el=id=>document.getElementById(id);let data=null,online=false,received=0,busy=false;
 function paint(){
  const r=reading(data,online,Date.now()-received);
  el('header-boiler-clock').textContent=r.time;el('header-clock').title=r.fresh?'Zegar odczytany z panelu kotła · '+r.date:'Brak świeżego odczytu zegara kotła';
  el('boiler-clock-time').textContent=r.time;el('boiler-clock-date').textContent=r.date;
  if(!busy)el('boiler-clock-auto').checked=!!data?.automatic;
  el('boiler-clock-auto').disabled=busy||!online||!data?.supported;
  el('boiler-clock-sync').disabled=busy||!online||!data?.available||!!data?.pending;
  el('boiler-clock-status').textContent=!online?'Brak połączenia z ESP':data?.message||'Brak odczytu zegara';
  el('boiler-clock-source').textContent=data?.automatic?(data.ntp_synchronized?'Czas internetowy NTP jest aktualny.':'Oczekiwanie na czas z internetu. Bez niego automat nie zmienia zegara.'):'Synchronizacja automatyczna jest wyłączona.';
 }
 async function save(payload){
  if(busy)return;busy=true;paint();el('boiler-clock-error').textContent='';
  try{
   if(payload.action==='sync')await request('/api/panel/clock',{epoch:Date.now()/1000});
   data=await request('/api/boiler-clock',payload);received=Date.now();await poll();
  }catch(error){el('boiler-clock-error').textContent=error.message;}
  finally{busy=false;paint();}
 }
 function init(){
  el('boiler-clock-auto').onchange=()=>save({automatic:el('boiler-clock-auto').checked});
  el('boiler-clock-sync').onclick=()=>save({action:'sync'});setInterval(paint,1000);paint();
 }
 root.EcoBoilerClock={init,render(value,connected){data=value;online=connected;received=Date.now();paint();}};
})(globalThis);
