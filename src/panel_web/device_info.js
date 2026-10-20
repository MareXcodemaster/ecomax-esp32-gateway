'use strict';
(function(root){
 const valid=n=>typeof n==='number'&&Number.isFinite(n)&&n>=0;
 const fmt=(n,digits=1)=>n.toLocaleString('pl-PL',{maximumFractionDigits:digits});
 function bytes(n){
  if(!valid(n))return '---';
  const units=['B','KiB','MiB','GiB'];let index=0;
  while(n>=1024&&index<units.length-1){n/=1024;index++;}
  return fmt(n,index?2:0)+' '+units[index];
 }
 function duration(ms){
  if(!valid(ms))return '---';
  const seconds=Math.floor(ms/1000),days=Math.floor(seconds/86400),hours=Math.floor(seconds/3600)%24,minutes=Math.floor(seconds/60)%60;
  return (days?days+' d ':'')+String(hours).padStart(2,'0')+':'+String(minutes).padStart(2,'0')+':'+String(seconds%60).padStart(2,'0');
 }
 const resets=['Nieznana','Wlaczenie zasilania','Reset zewnetrzny','Restart programowy','Awaria programu (panic)','Watchdog przerwan','Watchdog zadania','Watchdog','Wybudzenie z uspienia','Spadek napiecia zasilania','Reset SDIO','Reset USB','Reset JTAG','Blad eFuse','Zaklocenie zasilania','Zablokowanie CPU'];
 const reset=n=>Number.isInteger(n)&&n>=0?(resets[n]||'Kod '+n):'---';
 function usage(value){
  if(!value||!valid(value.total_bytes)||!valid(value.used_bytes)||!valid(value.free_bytes)||value.total_bytes<=0||value.used_bytes>value.total_bytes||value.free_bytes>value.total_bytes||value.used_bytes+value.free_bytes!==value.total_bytes)return null;
  return {...value,percent:value.used_bytes/value.total_bytes*100};
 }
 function model(data,connected){
  if(!connected||data?.schema!==1||!valid(data.uptime_ms))return null;
  const history=data.history?.ready===true?data.history:null;
  return {
   pools:{firmware:usage(data.firmware),ram:usage(data.internal_ram),psram:usage(data.psram),history:usage(history&&{...history,free_bytes:history.total_bytes-history.used_bytes})},
   details:[
    ['Uklad',typeof data.chip==='string'?data.chip:'---'],
    ['Rewizja ukladu',Number.isInteger(data.chip_revision)&&data.chip_revision>=0?Math.floor(data.chip_revision/100)+'.'+data.chip_revision%100:'---'],
    ['Rdzenie CPU',valid(data.cpu_cores)?String(data.cpu_cores):'---'],
    ['Taktowanie CPU',valid(data.cpu_mhz)?fmt(data.cpu_mhz,0)+' MHz':'---'],
    ['Flash urzadzenia',bytes(data.flash_total_bytes)],
    ['Aktywny slot programu',typeof data.firmware?.partition==='string'?data.firmware.partition:'---'],
    ['ESP-IDF',typeof data.idf_version==='string'?data.idf_version:'---'],
    ['Czas pracy od restartu',duration(data.uptime_ms)],
    ['Przyczyna ostatniego restartu',reset(data.reset_reason)],
    ['Najmniej wolnego RAM od startu',bytes(data.internal_ram?.minimum_free_bytes)],
    ['Najwiekszy wolny blok RAM',bytes(data.internal_ram?.largest_free_block_bytes)],
    ['Najmniej wolnego PSRAM od startu',bytes(data.psram?.minimum_free_bytes)],
    ['Najwiekszy wolny blok PSRAM',bytes(data.psram?.largest_free_block_bytes)],
    ['Probki w archiwum flash',valid(history?.samples)?fmt(history.samples,0):'---'],
    ['Retencja historii',valid(history?.retention_days)?fmt(history.retention_days,0)+' dni':'---']
   ],history_error:history?'':data.history?.loading?'Archiwum: uruchamianie':typeof data.history?.error==='string'&&data.history.error?'Archiwum: '+data.history.error:'Archiwum: brak odczytu'
  };
 }
 if(typeof module==='object'&&module.exports)module.exports={bytes,duration,reset,usage,model};
 if(typeof document==='undefined')return;
 function render(data,connected){
  const get=id=>document.getElementById(id),view=model(data,connected);
  get('esp-resources-status').textContent=view?'Aktualne dane ESP':'Brak aktualnego odczytu ESP';
  for(const key of ['firmware','ram','psram','history']){
   const pool=view?.pools[key],meter=get('esp-memory-'+key),summary=get('esp-memory-'+key+'-used'),free=get('esp-memory-'+key+'-free');
   meter.hidden=!pool;
   if(pool){
    meter.max=pool.total_bytes;meter.value=pool.used_bytes;meter.low=.7*pool.total_bytes;meter.high=.9*pool.total_bytes;meter.optimum=0;
    const value=bytes(pool.used_bytes)+' / '+bytes(pool.total_bytes)+' ('+fmt(pool.percent)+'%)';
    meter.setAttribute('aria-valuetext',value);summary.textContent=value;free.textContent='Wolne: '+bytes(pool.free_bytes);
   }else{meter.removeAttribute('aria-valuetext');summary.textContent='Brak odczytu';free.textContent='---';}
  }
  const list=get('esp-device-values'),signature=JSON.stringify(view?.details||[]);
  if(list.dataset.signature!==signature){
   list.replaceChildren();
   for(const [name,value]of view?.details||[]){
    const row=document.createElement('div'),dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=name;dd.textContent=value;row.append(dt,dd);list.append(row);
   }
   list.dataset.signature=signature;
  }
  get('esp-history-storage-status').textContent=view?.history_error||'';
 }
 root.EcoDevice={render};
})(globalThis);
