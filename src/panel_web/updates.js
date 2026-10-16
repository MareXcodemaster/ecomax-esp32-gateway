'use strict';
(function(root){
 function stableOffer(data){return !!(data?.available&&typeof data.version==='string'&&/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(data.version));}
 if(typeof module!=='undefined'&&module.exports)module.exports={stableOffer};
 if(typeof document==='undefined')return;
 const $=id=>document.getElementById(id);
 let data=null,saving=false,polling=false,expectRestart=false,operationError='',expectedVersion='',monitorUntil=0,timer;
 function render(){
  if(!data)return;
  const offer=stableOffer(data),busy=saving||data.busy;
  $('update-current').textContent=data.current||'---';
  $('update-latest').textContent=data.version||(data.phase==='no_stable_release'?'Brak stabilnego wydania':data.phase==='error'?'Brak aktualnego odczytu':'Jeszcze nie sprawdzono');
  $('update-auto').checked=!!data.automatic;$('update-auto').disabled=saving;
  $('update-check').disabled=!!busy;$('update-install').hidden=!offer;
  $('update-install').disabled=!!busy||!data.install_supported;
  $('update-private').hidden=!!data.install_supported;
  $('update-notice').hidden=!offer;$('update-notice-version').textContent=data.version||'';
  if(['downloading','restarting'].includes(data.phase)||expectedVersion&&data.current===expectedVersion)operationError='';
  if(expectedVersion&&data.current===expectedVersion&&!data.busy&&data.phase!=='starting'){expectRestart=false;expectedVersion='';location.reload();return;}
  const messages={idle:'',checking:'Sprawdzanie GitHuba...',waiting_for_wifi:'Oczekiwanie na po\u0142\u0105czenie Wi-Fi.',waiting_for_time:'Oczekiwanie na prawid\u0142owy czas do weryfikacji HTTPS.',no_stable_release:'Nie ma jeszcze stabilnego wydania. Wersje beta s\u0105 pomijane.',up_to_date:'Nie ma nowszej stabilnej wersji.',available:'Dost\u0119pna nowa stabilna wersja.',downloading:'Pobieranie i sprawdzanie obrazu OTA...',restarting:'Obraz sprawdzony. ESP uruchamia si\u0119 ponownie.'};
  $('update-status').textContent=operationError||data.error||messages[data.phase]||'';
  $('update-progress').hidden=data.phase!=='downloading';$('update-progress').max=data.size||1;$('update-progress').value=data.received||0;
  $('update-checked').textContent=data.checked?'Sprawdzono: '+new Date(data.checked*1000).toLocaleString('pl-PL'):'';
  if(data.phase==='error')expectRestart=false;
 }
 async function load(){
  clearTimeout(timer);
  if(polling||!csrf||!serverReachable){timer=setTimeout(load,5000);return;}
  polling=true;
  try{data=await request('/api/updates');render();}
  catch(error){$('update-status').textContent=expectRestart?'Sprawdzam powr\u00f3t ESP po aktualizacji...':error.message;}
  finally{polling=false;if(expectRestart&&Date.now()>monitorUntil)expectRestart=false;timer=setTimeout(load,data?.busy||expectRestart?2000:60000);}
 }
 function init(){
  $('update-open').onclick=()=>{selectView('settings');load();};
  $('update-check').onclick=async()=>{
   if(saving)return;saving=true;operationError='';render();
   try{await request('/api/updates/check',{});}catch(error){operationError=error.message;}
   finally{saving=false;await load();}
  };
  $('update-auto').onchange=async()=>{
   if(saving)return;const value=$('update-auto').checked;saving=true;operationError='';
   try{await request('/api/updates/config',{automatic:value});}
   catch(error){operationError=error.message;}
   finally{saving=false;await load();}
  };
  $('update-install').onclick=async()=>{
   if(saving||!stableOffer(data)||!data.install_supported)return;
   const version=data.version;
   if(!confirm('Zaktualizowa\u0107 ESP do stabilnej wersji '+version+'? Nast\u0105pi kr\u00f3tki restart ESP. Nie od\u0142\u0105czaj zasilania.'))return;
   saving=true;operationError='';expectedVersion=version;monitorUntil=Date.now()+300000;render();
   try{await request('/api/updates/install',{version,confirm:true});expectRestart=true;}
   catch(error){operationError='Nie potwierdzono przyj\u0119cia aktualizacji. Sprawd\u017a stan ESP przed ponownym poleceniem. '+error.message;expectRestart=!Number.isInteger(error.status)||error.status>=500;}
   finally{saving=false;await load();}
  };
  timer=setTimeout(load,3000);
 }
 root.EcoUpdates={init,load};
})(globalThis);
