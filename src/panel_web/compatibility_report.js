'use strict';
(function(root){
 function validate(report,id){
  if(report?.format!=='ecomax-compatibility'||report.schema!==1||report.read_only!==true||report.report_id!==id||!Array.isArray(report.read_results)||!Array.isArray(report.warnings))throw Error('Nieprawidłowy raport zgodności. Odczytaj kocioł ponownie.');
  return report;
 }
 const reasons={
  unsupported_model:'Ten model nie ma jeszcze profilu zapisu. Pobierz raport zgodności i prześlij go autorowi projektu.',
  identity_mismatch:'ESP pamięta inny kocioł. Pobierz raport zgodności, aby wyjaśnić blokadę zapisu.',
  invalid_identity:'Nie udało się potwierdzić identyfikacji kotła. Pobierz raport zgodności do sprawdzenia.'
 };
 const api={validate,reasons};root.EcoCompatibility=api;
 if(typeof module==='object'&&module.exports)module.exports=api;
 if(typeof document==='undefined')return;
 document.addEventListener('DOMContentLoaded',()=>{
  const get=id=>document.getElementById(id),dialog=get('compatibility-dialog');let collecting=false;
  const open=()=>{get('compatibility-status').textContent='';dialog.showModal();};
  for(const id of ['compatibility-open','compatibility-settings','compatibility-blocked-open'])get(id).onclick=open;
  get('compatibility-close').onclick=()=>{if(!collecting)dialog.close();};
  dialog.addEventListener('cancel',event=>{if(collecting)event.preventDefault();});
  api.render=(value,reachable)=>{
   const reason=reasons[value?.identity_status];
   get('compatibility-blocked').hidden=!reason;
   get('compatibility-reason').textContent=reason||'';
   get('compatibility-controller').textContent=value?.observed_controller||'Brak potwierdzonej identyfikacji';
   get('compatibility-start').disabled=collecting||!reachable||value?.busy===true;
   if(collecting){const step=value?.job?.kind==='compatibility'?value.job.step:'';get('compatibility-status').textContent=step||'Odczytywanie kotła… Może potrwać około minuty.';}
  };
  get('compatibility-form').onsubmit=async event=>{
   event.preventDefault();if(collecting)return;collecting=true;
   get('compatibility-start').disabled=true;get('compatibility-close').disabled=true;get('compatibility-active').disabled=true;
   get('compatibility-status').textContent='Odczytywanie kotła… Może potrwać około minuty.';
   try{
    const result=await run('/api/compatibility/scan',{active_reads:get('compatibility-active').checked});
    if(!result?.report_id)throw Error('ESP nie zwróciło identyfikatora raportu.');
    const report=validate(await request('/api/compatibility?id='+encodeURIComponent(result.report_id)),result.report_id);
    const blob=new Blob([JSON.stringify(report,null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),link=document.createElement('a');
    link.href=url;link.download='ecomax-zgodnosc-'+new Date().toISOString().replace(/[:.]/g,'-')+'.json';document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);
    get('compatibility-status').textContent=(report.partial?'Pobrano raport częściowy — również można go przesłać.':'Pobrano raport zgodności.')+' Odpowiedzi: '+report.received_replies+'. Nastaw nie zmieniono.';
   }catch(error){get('compatibility-status').textContent=error.message;}
   finally{collecting=false;get('compatibility-close').disabled=false;get('compatibility-active').disabled=false;get('compatibility-start').disabled=false;}
  };
 });
})(globalThis);
