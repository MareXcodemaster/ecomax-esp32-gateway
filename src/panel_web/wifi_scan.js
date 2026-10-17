(function(root) {
 'use strict';
 function networks(result) {
  const unique=new Map();
  for(const item of Array.isArray(result?.networks)?result.networks.slice(0,64):[]) {
   if(typeof item.ssid!=='string'||!item.ssid||new TextEncoder().encode(item.ssid).length>32||/[\x00-\x1f\x7f]/.test(item.ssid)||!Number.isFinite(item.rssi)||item.rssi<-127||item.rssi>0||!Number.isInteger(item.authmode)||item.authmode<0)continue;
   if(!unique.has(item.ssid)||unique.get(item.ssid).rssi<item.rssi)unique.set(item.ssid,{ssid:item.ssid,rssi:item.rssi,open:item.authmode===0});
  }
  return [...unique.values()].sort((a,b)=>b.rssi-a.rssi||a.ssid.localeCompare(b.ssid)).slice(0,16);
 }
 async function scan(read,start,delay=ms=>new Promise(done=>setTimeout(done,ms))) {
  const before=await read();
  if(!Number.isInteger(before.sequence))throw new Error('Firmware nie udostepnia wynikow skanowania.');
  await start();
  for(let attempt=0;attempt<50;attempt++) {
   await delay(400);
   const value=await read();
   if(!value.busy&&Number.isInteger(value.sequence)&&value.sequence!==before.sequence) {
    if(value.result!=='ESP_OK')throw new Error('Nie udalo sie wyszukac sieci. Zaczekaj na zakonczenie laczenia i sprobuj ponownie.');
    return networks(value);
   }
  }
  throw new Error('Wyszukiwanie przekroczylo czas. Nie ponowiono automatycznie; mozesz wpisac nazwe recznie.');
 }
 function options(select,items,current='') {
  select.replaceChildren(new Option(items.length?'Wybierz swoja siec':'Nie znaleziono sieci',''));
  for(const item of items)select.append(new Option(item.ssid+' / '+item.rssi+' dBm / '+(item.open?'otwarta':'zabezpieczona'),item.ssid));
  // The sentinel cannot collide with a network name: selection uses its own option index.
  const manual=new Option('Siec ukryta / wpisz nazwe recznie','');manual.dataset.manual='true';select.append(manual);
  const selected=[...select.options].find(option=>option.value===current&&!option.dataset.manual&&current);
  if(selected)selected.selected=true;
 }
 const api={networks,scan,options};
 if(typeof module==='object'&&module.exports)module.exports=api;
 root.PiecWifiScan=api;
})(globalThis);
