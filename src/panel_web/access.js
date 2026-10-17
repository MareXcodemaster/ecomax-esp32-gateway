'use strict';
(function(root){
 function valid(value,min){return typeof value==='string'&&value.length>=min&&value.length<=48&&/^[\x20-\x7e]+$/.test(value);}
 function draft(current,password,repeat,ap,apRepeat){
  if(!valid(current,5)||!valid(password,5))throw Error('Haslo panelu: 5-48 znakow ASCII.');
  if(password!==repeat)throw Error('Hasla panelu nie sa identyczne.');
  if(ap!==apRepeat)throw Error('Hasla Piec-Setup nie sa identyczne.');
  if(ap&&!valid(ap,8))throw Error('Haslo Piec-Setup: 8-48 znakow ASCII.');
  return {current_password:current,password,ap_password:ap||null,confirm:true};
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={valid,draft};
 if(typeof document==='undefined')return;
 const $=id=>document.getElementById(id),ids=['access-current','access-password','access-repeat','access-ap','access-ap-repeat'];
 let saving=false;
 function init(){
  $('access-form').onsubmit=async event=>{
   event.preventDefault();if(saving)return;
   let payload;
   try{payload=draft(...ids.map(id=>$(id).value));}catch(error){$('access-status').textContent=error.message;return;}
   if(!confirm('Zapisac hasla i zrestartowac ESP? Po restarcie zaloguj sie jako admin nowym haslem.'))return;
   saving=true;$('access-save').disabled=true;$('access-status').textContent='Zapisywanie...';
   try{const reply=await request('/api/access',payload);ids.forEach(id=>$(id).value='');$('access-status').textContent=reply.message||'Hasla zapisane. Restart ESP; zaloguj sie nowym haslem.';}
   catch(error){ids.forEach(id=>$(id).value='');$('access-status').textContent='Nie ponowiono zapisu. Sprawdz stan ESP i logowanie. '+error.message;}
   finally{payload=null;saving=false;$('access-save').disabled=false;}
  };
 }
 root.EcoAccess={init};
})(globalThis);
