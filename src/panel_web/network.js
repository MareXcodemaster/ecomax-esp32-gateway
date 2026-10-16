'use strict';
(function(root){
 const validHostname=name=>typeof name==='string'&&name.length<=63&&/^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/.test(name)&&name!=='localhost';
 const normalizeHostname=name=>name.trim().toLowerCase().replace(/\.local\.?$/,'');
 if(typeof module!=='undefined'&&module.exports)module.exports={validHostname,normalizeHostname};
 if(typeof document==='undefined')return;
 const $=id=>document.getElementById(id);
 let dirty=false,saving=false;
 function render(network={}){
  const name=validHostname(network.hostname)?network.hostname:'piec';
  if(!dirty&&document.activeElement!==$('esp-hostname'))$('esp-hostname').value=name;
  if(state?.write_transport==='rs485')$('address').textContent=name+'.local / RS485';
  if(!saving)$('esp-hostname-save').disabled=!!network.hostname_pending;
 }
 function init(){
  $('esp-hostname').addEventListener('input',()=>{dirty=true;});
  $('esp-hostname-form').onsubmit=async event=>{
   event.preventDefault();if(saving)return;
   const name=normalizeHostname($('esp-hostname').value),status=$('esp-hostname-status'),link=$('esp-hostname-link');
   if(!validHostname(name)){status.textContent='Nazwa: 1-63 znaki, litery a-z, cyfry i my\u015blnik (nie na pocz\u0105tku ani ko\u0144cu).';return;}
   saving=true;$('esp-hostname-save').disabled=true;link.hidden=true;status.textContent='Zapisywanie...';
   try{
    await request('/api/hostname',{hostname:name});
    $('esp-hostname').value=name;dirty=false;
    status.textContent='Zapisano. Nowy adres: ';
    link.href='http://'+name+'.local/';link.textContent=name+'.local';link.hidden=false;
   }catch(error){status.textContent=error.message;}
   finally{saving=false;$('esp-hostname-save').disabled=false;}
  };
 }
 root.EcoNetwork={init,render};
})(globalThis);
