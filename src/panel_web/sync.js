(function(root){
 'use strict';
 function create(){
  let token='',catalog=null;
  const own=(o,k)=>Object.prototype.hasOwnProperty.call(o||{},k);
  function reset(){token='';catalog=null;}
  function merge(value){
   if(!value?.sync){reset();return value;}// Compatibility with older ESP and exports.
   const sync=value.sync;
   if(sync.schema!==1||typeof sync.catalog!=='string'||!/^[a-f0-9]{16}$/.test(sync.catalog))throw Error('Nieprawidlowy format odczytu ESP');
   const definitions=sync.included?value.catalog:sync.catalog===token?catalog:null;
   if(!definitions||!Array.isArray(definitions.menu)||!definitions.targets||!Array.isArray(value.menu)||!value.targets)throw Error('Brak aktualnego katalogu ESP');
   const byIndex=new Map(definitions.menu.map(row=>[JSON.stringify(row.index),row]));
   const menu=value.menu.map(row=>{
    const base=byIndex.get(JSON.stringify(row.index));if(!base)throw Error('Zmieniony katalog menu ESP');
    const result={...base,...row};
    if(row.metadata&&typeof row.metadata==='object')result.metadata={...base.metadata,...row.metadata};
    return result;
   });
   const targets={};for(const [key,row]of Object.entries(value.targets)){if(!own(definitions.targets,key))throw Error('Zmieniony katalog nastaw ESP');targets[key]={...definitions.targets[key],...row};}
   const next={...definitions,...value,menu,targets};delete next.catalog;
   // Commit only a fully validated response; never mix identities or revisions.
   catalog=definitions;token=sync.catalog;return next;
  }
  async function load(request){
   const value=await request('/api/state?compact=1'+(token?'&catalog='+token:''));
   try{return merge(value);}catch(error){reset();return merge(await request('/api/state?compact=1'));}
  }
  return {load,merge,reset,get token(){return token;}};
 }
 const api=create();api.create=create;root.EcoSync=api;
 if(typeof module==='object'&&module.exports)module.exports={create};
})(globalThis);
