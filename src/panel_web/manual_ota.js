(function(root){
 'use strict';
 function inspect(bytes,size,name){
  if(/merged/i.test(name)||size<1024||size>3*1024*1024)throw new Error('Wybierz firmware-ota.bin (do 3 MiB). Obraz merged służy do instalacji przez USB.');
  const data=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  if(bytes.length<112||bytes[0]!==0xe9||data.getUint16(12,true)!==9||data.getUint32(32,true)!==0xabcd5432)throw new Error('To nie jest obraz aplikacji OTA dla ESP32-S3.');
  const text=at=>new TextDecoder().decode(bytes.slice(at,at+32)).split('\0')[0];
  if(text(80)!=='lazar_stage0')throw new Error('Plik pochodzi z innego projektu.');
  const version=text(48);if(!/^\d+\.\d+\.\d+(?:[-.\w]+)?$/.test(version))throw new Error('Nie rozpoznano wersji firmware.');
  return version;
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={inspect};
 if(typeof document==='undefined')return;
 const el=id=>document.getElementById(id);let file=null,version='',busy=false,selection=0;
 function status(s){el('manual-ota-status').textContent=s;}
 function paint(){el('manual-ota-file').disabled=busy;el('manual-ota-confirm').disabled=busy||!file;el('manual-ota-install').disabled=busy||!file||!el('manual-ota-confirm').checked;}
 async function monitor(before,started){
  const until=Date.now()+150000;await new Promise(r=>setTimeout(r,5000));
  while(Date.now()<until){
   try{const response=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(6000)});const info=await response.json();
    if(response.ok&&info.firmware===version&&Number.isFinite(info.uptime_ms)&&info.uptime_ms<before+Date.now()-started-2000){status('Aktualizacja potwierdzona: '+version+'. Otwieram panel…');setTimeout(()=>location.reload(),1000);return;}
   }catch{}
   await new Promise(r=>setTimeout(r,3000));
  }
  status('Nie potwierdzono powrotu ESP. Otwórz panel ponownie i sprawdź wersję przed kolejną próbą. Pliku nie wysłano ponownie.');busy=false;actionBusy=false;paint();
 }
 async function install(){
  if(busy||actionBusy||state?.busy||!file||!el('manual-ota-confirm').checked)return;
  busy=true;actionBusy=true;actionEpoch++;paint();const progress=el('manual-ota-progress');progress.hidden=false;progress.value=0;status('Sprawdzanie połączenia z ESP…');
  let before;try{const info=await request('/api/status');before=info.uptime_ms;if(!Number.isFinite(before))throw new Error('Brak czasu pracy ESP');}
  catch(error){status(error.message);busy=false;actionBusy=false;paint();return;}
  const started=Date.now(),xhr=new XMLHttpRequest();xhr.open('POST','/api/ota');xhr.timeout=180000;
  xhr.setRequestHeader('Content-Type','application/octet-stream');xhr.setRequestHeader('X-Lazar-Request','1');xhr.setRequestHeader('X-Tester-Token',csrf);
  xhr.upload.onprogress=event=>{if(event.lengthComputable){progress.value=Math.round(event.loaded/event.total*100);status('Wysyłanie pliku: '+progress.value+'%. Nie odłączaj zasilania.');}};
  const uncertain=()=>{status('Połączenie przerwane. Sprawdzam, czy ESP zakończyło aktualizację…');monitor(before,started);};
  xhr.onerror=uncertain;xhr.ontimeout=uncertain;
  xhr.onload=()=>{if(xhr.status===200){progress.value=100;status('Plik sprawdzony. ESP uruchamia się ponownie…');monitor(before,started);}else{let message='HTTP '+xhr.status;try{message=JSON.parse(xhr.responseText).message||message;}catch{}status('Nie zainstalowano: '+message);busy=false;actionBusy=false;paint();}};
  xhr.send(file);
 }
 function init(){
  el('manual-ota-file').onchange=async()=>{const id=++selection;file=null;version='';el('manual-ota-confirm').checked=false;paint();const selected=el('manual-ota-file').files[0];if(!selected){status('');return;}
   try{const bytes=new Uint8Array(await selected.slice(0,112).arrayBuffer());if(id!==selection)return;version=inspect(bytes,selected.size,selected.name);file=selected;status('Wybrano '+version+' · '+(file.size/1024/1024).toFixed(2)+' MiB.');}
   catch(error){if(id===selection)status(error.message);}paint();};
  el('manual-ota-confirm').onchange=paint;el('manual-ota-install').onclick=install;paint();
 }
 root.EcoManualOta={init,get busy(){return busy;}};
})(globalThis);
