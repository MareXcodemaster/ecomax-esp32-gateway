'use strict';
(function(root){
 const validSsid=value=>typeof value==='string'&&value.length>0&&new TextEncoder().encode(value).length<=32&&!/[\x00-\x1f\x7f]/.test(value);
 const validPassword=(value,open)=>open?value==='':typeof value==='string'&&((new TextEncoder().encode(value).length>=8&&new TextEncoder().encode(value).length<=63)||/^[a-fA-F0-9]{64}$/.test(value));
 if(typeof module==='object'&&module.exports)module.exports={validSsid,validPassword};
 if(typeof document==='undefined')return;
 const $=id=>document.getElementById(id);
 let network={},items=[],scanning=false,saving=false;
 const busy=()=>saving||scanning||actionBusy||state?.busy||!!ticket||!!controlTicket;
 const manual=()=>!!$('wifi-select').selectedOptions[0]?.dataset.manual;
 function controls(){
  const isManual=manual(),open=isManual?$('wifi-open-network').checked:!!items.find(item=>item.ssid===$('wifi-select').value)?.open;
  $('wifi-manual-field').hidden=!isManual;$('wifi-ssid').required=isManual;$('wifi-select').required=!isManual;
  $('wifi-open-field').hidden=!isManual;$('wifi-password').disabled=open;$('wifi-password').required=!open;
  $('wifi-search').disabled=busy();$('wifi-save').disabled=busy();
  for(const id of ['wifi-select','wifi-ssid','wifi-open-network'])$(id).disabled=saving||scanning;
  return open;
 }
 function render(value={}){network=value;if($('wifi-dialog').open){$('wifi-current').textContent=network.connected?'Obecna sie\u0107: '+(network.ssid||'---')+' / '+(network.ip||'---'):'ESP nie jest po\u0142\u0105czone z routerem';controls();}}
 function clearPassword(){$('wifi-password').value='';$('wifi-password').type='password';$('wifi-show-password').checked=false;}
 function open(){
  if($('wifi-dialog').open)return;
  clearPassword();$('wifi-result').textContent='';$('wifi-dialog').showModal();render(network);
 }
 function init(){
  $('wifi-open').onclick=open;$('wifi-settings-open').onclick=open;
  $('wifi-close').onclick=()=>{clearPassword();$('wifi-dialog').close();};
  $('wifi-dialog').addEventListener('cancel',clearPassword);
  $('wifi-dialog').addEventListener('close',clearPassword);
  $('wifi-select').onchange=()=>{clearPassword();controls();};$('wifi-open-network').onchange=()=>{clearPassword();controls();};
  $('wifi-show-password').onchange=()=>{$('wifi-password').type=$('wifi-show-password').checked?'text':'password';};
  $('wifi-search').onclick=async()=>{
   if(busy())return;scanning=true;controls();$('wifi-result').textContent='Wyszukiwanie sieci...';
   try{
    items=await PiecWifiScan.scan(()=>request('/api/wifi/scan'),()=>request('/api/wifi/scan',{}));
    PiecWifiScan.options($('wifi-select'),items,network.ssid);
    $('wifi-result').textContent=items.length?'Wybierz sie\u0107 i wpisz jej has\u0142o.':'Nie znaleziono sieci. Mo\u017cesz wpisa\u0107 nazw\u0119 r\u0119cznie.';
   }catch(error){$('wifi-result').textContent=error.message;}
   finally{scanning=false;controls();}
  };
  $('wifi-form').onsubmit=async event=>{
   event.preventDefault();if(busy())return;
   const ssid=manual()?$('wifi-ssid').value:$('wifi-select').value,openNetwork=controls(),password=openNetwork?'':$('wifi-password').value;
   if(!validSsid(ssid)){$('wifi-result').textContent='Wybierz sie\u0107 lub wpisz nazw\u0119 do 32 bajt\u00f3w.';return;}
   if(!validPassword(password,openNetwork)){$('wifi-result').textContent='Has\u0142o: 8-63 bajty lub 64 znaki szesnastkowe.';return;}
   saving=true;controls();$('wifi-result').textContent='Zapisywanie sieci...';
   try{
    await request('/api/wifi',{ssid,password});
    $('wifi-result').textContent='Zapisano sie\u0107. ESP rozpoczyna \u0142\u0105czenie. Po\u0142\u0105cz si\u0119 z now\u0105 sieci\u0105 i otw\u00f3rz '+(network.hostname||'piec')+'.local. W razie braku po\u0142\u0105czenia u\u017cyj sieci Piec-Setup.';
   }catch(error){$('wifi-result').textContent='Brak potwierdzenia zmiany: '+error.message+'. Nie ponowiono polecenia. Sprawd\u017a now\u0105 sie\u0107 lub Piec-Setup przed kolejnym zapisem.';}
   finally{saving=false;clearPassword();controls();}
  };
 }
 root.EcoWifi={init,render};
})(globalThis);
