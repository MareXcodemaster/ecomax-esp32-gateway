'use strict';
(function(root){
 function count(data,fresh){
  if(!fresh)return null;
  if(data?.write_transport==='rs485'){
   const s=data.alarm_status||{},n=s.pending_count;
   return s.count_fresh===true&&Number.isInteger(n)&&n>=0&&n<=65535&&(s.count_age_seconds===undefined||Number.isFinite(s.count_age_seconds)&&s.count_age_seconds>=0&&s.count_age_seconds<15)?n:null;
  }
  if(!Number.isFinite(data?.system_age_seconds)||data.system_age_seconds<0||data.system_age_seconds>=45||!Array.isArray(data.alarms))return null;
  return data.alarms.filter(a=>typeof a==='number'||a&&typeof a==='object'&&a.active!==false).length;
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={count};
 if(typeof document==='undefined')return;
 function render(data,fresh){
  const n=count(data,fresh),badge=document.getElementById('alarm-count');
  badge.hidden=n===null||n===0;badge.textContent=badge.hidden?'':n>999?'999+':String(n);badge.dataset.wide=String(n>99);
  badge.title=n===null?'Brak aktualnego odczytu alarmow':'Aktywne alarmy: '+n;
  badge.parentElement.setAttribute('aria-label',n===null?'Alarmy: brak aktualnego odczytu':'Alarmy: aktywne '+n);
 }
 root.EcoAlarmBadge={render};
})(globalThis);
