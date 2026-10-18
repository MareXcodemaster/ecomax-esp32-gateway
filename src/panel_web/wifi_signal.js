'use strict';
(function(root){
 function signal(network,reachable=true){
  const known=reachable&&network?.connected&&Number.isFinite(network.rssi_dbm);
  const percent=known?Math.max(0,Math.min(100,Math.round(2*(network.rssi_dbm+100)))):null;
  return {percent,bars:percent===null?0:Math.ceil(percent/20),label:percent===null?'Wi-Fi ESP: brak aktualnego odczytu':'Wi-Fi ESP: '+percent+'% (szacowane), '+network.rssi_dbm+' dBm'};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={signal};
 root.EcoWifiSignal={render(network,reachable){
  const button=document.getElementById('wifi-signal');if(!button)return;
  const value=signal(network,reachable);button.title=value.label;button.setAttribute('aria-label',value.label);
  button.querySelector('.wifi-percent').textContent=value.percent===null?'--%':value.percent+'%';
  button.querySelectorAll('.wifi-bars span').forEach((bar,i)=>bar.classList.toggle('active',i<value.bars));
  button.onclick=()=>document.getElementById('wifi-open').click();
 }};
})(typeof window==='undefined'?globalThis:window);
