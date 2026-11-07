'use strict';
(function(root){
 const phases={disabled:'Wyłączony',starting:'Uruchamianie…',stopping:'Rozłączanie…',waiting_wifi:'Oczekiwanie na Wi-Fi',waiting_time:'Oczekiwanie na synchronizację czasu',waiting_update:'Oczekiwanie na zakończenie aktualizacji',connecting:'Łączenie z Tailscale…',registering:'Rejestrowanie urządzenia…',waiting_policy:'Połączono — oczekiwanie na uprawnienia Tailnet',connected:'Połączono z Tailscale',connection_error:'Błąd połączenia',connect_timeout:'Nie udało się połączyć. Sprawdź klucz i dostęp do Internetu.',insufficient_memory:'Za mało wolnej pamięci na uruchomienie VPN.',memory_paused:'VPN zatrzymany: mały zapas pamięci. Panel lokalny nadal działa.',start_failed:'Nie udało się uruchomić VPN.',paused_after_fault:'VPN wstrzymany po awarii ESP. Możesz ponownie go uruchomić.',restart_required:'Nie zakończono połączenia VPN. Wymagany restart samego ESP.',unavailable:'Tailnet niedostępny'};
 function address(s){if(!s?.connected||!s.policy_ready||typeof s.ip!=='string')return '';const a=s.ip.split('.');return a.length===4&&a.every(x=>/^\d{1,3}$/.test(x)&&Number(x)<=255)&&Number(a[0])===100&&Number(a[1])>=64&&Number(a[1])<=127?'http://'+s.ip+'/':'';}
 function model(s){return {message:phases[s?.phase]||'Brak aktualnego odczytu',url:address(s),key:s?.key_saved?'Klucz zapisany w ESP. Pozostaw pole puste, aby go zachować.':'Wklej klucz autoryzacyjny utworzony na swoim koncie Tailscale.',memory:Number.isFinite(s?.internal_free_bytes)?Math.round(s.internal_free_bytes/1024)+' KiB wolnego RAM-u':''};}
 if(typeof module!=='undefined'&&module.exports)module.exports={model,address};
 if(typeof document==='undefined')return;
 const el=id=>document.getElementById(id);let data=null,dirty=false,busy=false,polling=false,timer;
 function render(){
  const m=model(data);el('tailnet-status').textContent=m.message;el('tailnet-key-status').textContent=m.key;
  el('tailnet-memory').textContent=m.memory;el('tailnet-link').hidden=!m.url;el('tailnet-link').textContent=m.url;
  if(m.url)el('tailnet-link').href=m.url;else el('tailnet-link').removeAttribute('href');
  if(data&&!dirty&&!busy){el('tailnet-enabled').checked=!!data.enabled;el('tailnet-name').value=data.name||'piec';}
  el('tailnet-save').disabled=busy||!data;el('tailnet-cancel').hidden=!dirty||busy;
  for(const id of ['tailnet-enabled','tailnet-name','tailnet-key'])el(id).disabled=busy;
 }
 async function load(){
  clearTimeout(timer);
  if(polling||busy||typeof view==='undefined'||view!=='settings'||!csrf||!serverReachable){timer=setTimeout(load,3000);return;}
  polling=true;try{data=await request('/api/tailnet');render();}
  catch(error){el('tailnet-status').textContent='Brak aktualnego połączenia z ESP';el('tailnet-link').hidden=true;}
  finally{polling=false;timer=setTimeout(load,5000);}
 }
 function init(){
  const edited=()=>{dirty=true;render();};
  el('tailnet-enabled').onchange=edited;el('tailnet-name').oninput=edited;el('tailnet-key').oninput=edited;
  el('tailnet-cancel').onclick=()=>{dirty=false;el('tailnet-key').value='';el('tailnet-message').textContent='';render();};
  el('tailnet-form').onsubmit=async event=>{
   event.preventDefault();if(busy||!data)return;
   const payload={enabled:el('tailnet-enabled').checked,name:el('tailnet-name').value.trim(),auth_key:el('tailnet-key').value.trim()};
   busy=true;el('tailnet-message').textContent='Zapisywanie…';render();
   try{data=await request('/api/tailnet',payload);dirty=false;el('tailnet-key').value='';el('tailnet-message').textContent='Ustawienia zapisane.';}
   catch(error){el('tailnet-message').textContent=error.message;}
   finally{payload.auth_key='';busy=false;render();load();}
  };
  render();timer=setTimeout(load,1000);
 }
 root.EcoTailnet={init,load};
})(globalThis);
