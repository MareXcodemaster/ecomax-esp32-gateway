'use strict';
(function(root){
 function stableOffer(data){return !!(data?.available&&typeof data.version==='string'&&/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(data.version));}
 function releaseOffer(data){return data?.version!==(data?.current_release||data?.current)&&(stableOffer(data)||!!(data?.available&&data.channel==='all'&&data.beta===true&&typeof data.version==='string'&&/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)-beta\.(0|[1-9]\d*)$/.test(data.version)));}
 function toolbarStatus(data,reachable=true,now=Date.now()){
  const release=data?.current_release||data?.current||'',age=now-Number(data?.checked)*1000;
  const offer=reachable&&!data?.error&&releaseOffer(data);
  const current=reachable&&!data?.error&&!data?.busy&&!data?.available&&data?.phase==='up_to_date'&&Number(data.checked)>0&&age>=-300000&&age<=25*3600000;
  return {state:offer?'available':current?'current':'unknown',label:offer?'Aktualizacja':current?'Aktualne':'Aktualizacje',version:offer?data.version:release,beta:offer?data.beta:current&&/-beta\.\d+$/.test(release)};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={stableOffer,releaseOffer,toolbarStatus};
 if(typeof document==='undefined')return;
 const $=id=>document.getElementById(id);
 let data=null,saving=false,polling=false,metadataReachable=false,expectRestart=false,operationError='',expectedVersion='',monitorUntil=0,timer;
 function render(){
  if(!data)return;
  const offer=metadataReachable&&releaseOffer(data),busy=saving||data.busy,toolbar=toolbarStatus(data,metadataReachable);
  $('update-current').textContent=(data.current_release||data.current||'---')+(data.current_release&&data.current!==data.current_release?' (kompilacja '+data.current+')':'');
  $('update-latest').textContent=data.version?(data.version+(data.beta?' \u00b7 BETA':'')):(['no_release','no_stable_release'].includes(data.phase)?'Brak wydania w wybranym kanale':data.phase==='error'?'Brak aktualnego odczytu':'Jeszcze nie sprawdzono');
  $('update-channel').value=data.channel||'stable';$('update-channel').disabled=!!busy;
  $('update-auto').checked=!!data.automatic;$('update-auto').disabled=saving;
  $('update-check').disabled=!!busy;$('update-install').hidden=!offer;
  $('update-install').disabled=!!busy||!data.install_supported;
  $('update-private').hidden=!!data.install_supported;
  $('update-notice').hidden=false;$('update-notice').dataset.state=toolbar.state;
  $('update-notice-version').textContent=toolbar.state==='unknown'?'Brak aktualnego potwierdzenia wersji z GitHuba.':toolbar.label+': '+toolbar.version+(toolbar.beta?' BETA':'');
  $('update-open-label').textContent=toolbar.label;$('update-offer-icon').hidden=toolbar.state==='current';$('update-current-icon').hidden=toolbar.state!=='current';
  $('update-notice-beta').hidden=!toolbar.beta;$('update-open').title=$('update-notice-version').textContent;
  if(['downloading','restarting'].includes(data.phase)||expectedVersion&&data.current===expectedVersion)operationError='';
  if(expectedVersion&&data.current===expectedVersion&&!data.busy&&data.phase!=='starting'){expectRestart=false;expectedVersion='';location.reload();return;}
  const messages={idle:'',checking:'Sprawdzanie GitHuba...',waiting_for_wifi:'Oczekiwanie na po\u0142\u0105czenie Wi-Fi.',waiting_for_time:'Oczekiwanie na prawid\u0142owy czas do weryfikacji HTTPS.',no_release:'Brak wydania w wybranym kanale.',no_stable_release:'Nie ma jeszcze stabilnego wydania. Mo\u017cesz wybra\u0107 stabilne i beta.',up_to_date:'Nie ma nowszej wersji w wybranym kanale.',available:data.beta?'Dost\u0119pna wersja BETA: wydanie testowe, mo\u017ce zawiera\u0107 b\u0142\u0119dy.':'Dost\u0119pna nowa stabilna wersja.',downloading:'Pobieranie i sprawdzanie obrazu OTA...',restarting:'Obraz sprawdzony. ESP uruchamia si\u0119 ponownie.'};
  $('update-status').textContent=operationError||data.error||messages[data.phase]||'';
  $('update-progress').hidden=data.phase!=='downloading';$('update-progress').max=data.size||1;$('update-progress').value=data.received||0;
  $('update-checked').textContent=data.checked?'Sprawdzono: '+new Date(data.checked*1000).toLocaleString('pl-PL'):'';
  if(data.phase==='error')expectRestart=false;
 }
 async function load(){
  clearTimeout(timer);
  if(polling){timer=setTimeout(load,5000);return;}
  if(!csrf||!serverReachable){metadataReachable=false;render();timer=setTimeout(load,5000);return;}
  polling=true;
  try{data=await request('/api/updates');metadataReachable=true;render();}
  catch(error){metadataReachable=false;render();$('update-status').textContent=expectRestart?'Sprawdzam powr\u00f3t ESP po aktualizacji...':error.message;}
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
   try{await request('/api/updates/config',{automatic:value,include_beta:$('update-channel').value==='all'});}
   catch(error){operationError=error.message;}
   finally{saving=false;await load();}
  };
  $('update-install').onclick=async()=>{
   if(saving||!metadataReachable||!releaseOffer(data)||!data.install_supported)return;
   const version=data.version;
   if(!confirm('Zaktualizowa\u0107 ESP do wersji '+version+(data.beta?' BETA (wydanie testowe, mo\u017ce zawiera\u0107 b\u0142\u0119dy)':' stabilnej')+'? Nast\u0105pi kr\u00f3tki restart ESP. Nie od\u0142\u0105czaj zasilania.'))return;
   saving=true;operationError='';expectedVersion=version;monitorUntil=Date.now()+300000;render();
   try{await request('/api/updates/install',{version,confirm:true});expectRestart=true;}
   catch(error){operationError='Nie potwierdzono przyj\u0119cia aktualizacji. Sprawd\u017a stan ESP przed ponownym poleceniem. '+error.message;expectRestart=!Number.isInteger(error.status)||error.status>=500;}
   finally{saving=false;await load();}
  };
  timer=setTimeout(load,3000);
 }
 root.EcoUpdates={init,load};
})(globalThis);
