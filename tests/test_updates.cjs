const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const source=fs.readFileSync(path.join(__dirname,'../src/panel_web/updates.js'),'utf8');
const {stableOffer,releaseOffer}=require('../src/panel_web/updates.js');
function ui(state,postError=false,confirmation=true){
 const elements={},requests=[],reloads=[];
 const ctx={document:{getElementById:id=>elements[id]??=( {textContent:'',hidden:false,disabled:false,checked:true} )},
  csrf:'test',serverReachable:true,setTimeout:()=>1,clearTimeout(){},confirm:()=>confirmation,selectView(){},location:{reload:()=>reloads.push(true)},
  request:async(url,payload)=>{requests.push({url,payload});if(payload&&postError)throw Error('Lost response');return state;}};
 vm.createContext(ctx);vm.runInContext(source,ctx);ctx.EcoUpdates.init();
 return {elements,requests,ctx,reloads};
}
const stable=()=>({current:'0.7.0-beta.3',version:'0.7.0',available:true,install_supported:true,automatic:true,phase:'available',busy:false});
test('only an actual stable version can produce an offer',()=>{
 assert.equal(stableOffer(stable()),true);
 for(const version of ['0.7.0-beta.4','v0.7.0','0.7','01.7.0','','<script>'])assert.equal(stableOffer({...stable(),version}),false);
 assert.equal(stableOffer({...stable(),available:false}),false);
});
test('beta is an explicitly labelled opt-in channel offer, installed only after confirmation',async()=>{
 const state={...stable(),channel:'all',version:'0.7.0-beta.5',beta:true};
 assert.equal(releaseOffer(state),true);assert.equal(releaseOffer({...state,channel:'stable'}),false);
 assert.equal(releaseOffer({...state,beta:false}),false);
 for(const version of ['0.7.0-beta.05','0.7.0-beta','0.7.0-unknown.1','<beta>'])assert.equal(releaseOffer({...state,version}),false);
 const view=ui(state);await view.ctx.EcoUpdates.load();
 assert.match(view.elements['update-latest'].textContent,/BETA/);assert.match(view.elements['update-status'].textContent,/testowe/);
 assert.equal(view.requests.filter(r=>r.payload).length,0);
 await view.elements['update-install'].onclick();
 assert.equal(view.requests.filter(r=>r.url==='/api/updates/install').length,1);
});
test('opening updater and refreshing never installs anything',async()=>{
 const view=ui({...stable(),available:false,version:'',phase:'no_stable_release'});await view.ctx.EcoUpdates.load();
 assert.equal(view.elements['update-install'].hidden,true);
 assert.equal(view.elements['update-notice'].hidden,true);
 assert.equal(view.requests.filter(r=>r.payload).length,0);
 assert.match(view.elements['update-status'].textContent,/beta/);
});
test('installation needs a stable offer, supported edition and confirmation',async()=>{
 for(const [state,confirm]of [[{...stable(),install_supported:false},true],[{...stable(),version:'0.8.0-beta.1'},true],[stable(),false]]){
  const view=ui(state,false,confirm);await view.ctx.EcoUpdates.load();await view.elements['update-install'].onclick();
  assert.equal(view.requests.filter(r=>r.payload).length,0);
 }
 const view=ui(stable());await view.ctx.EcoUpdates.load();await view.elements['update-install'].onclick();
 assert.equal(view.requests.filter(r=>r.url==='/api/updates/install').length,1);
 assert.equal(JSON.stringify(view.requests.find(r=>r.url==='/api/updates/install').payload),JSON.stringify({version:'0.7.0',confirm:true}));
});
test('unknown install response is monitored by GET only and remains visible',async()=>{
 const view=ui(stable(),true);await view.ctx.EcoUpdates.load();await view.elements['update-install'].onclick();
 await view.ctx.EcoUpdates.load();await view.ctx.EcoUpdates.load();
 assert.equal(view.requests.filter(r=>r.url==='/api/updates/install').length,1);
 assert.match(view.elements['update-status'].textContent,/Nie potwierdzono/);
});
test('panel reloads once only after the expected new firmware finishes startup',async()=>{
 const state=stable(),view=ui(state);await view.ctx.EcoUpdates.load();await view.elements['update-install'].onclick();
 state.current='0.7.0';state.phase='starting';state.busy=true;await view.ctx.EcoUpdates.load();assert.equal(view.reloads.length,0);
 state.phase='idle';state.busy=false;await view.ctx.EcoUpdates.load();await view.ctx.EcoUpdates.load();
 assert.equal(view.reloads.length,1);
});
test('firmware checks fixed GitHub stable endpoint, TLS, digest and project before selecting boot slot',()=>{
 const backend=fs.readFileSync(path.join(__dirname,'../src/updates.cpp'),'utf8');
 assert.match(backend,/releases\/latest/);assert.match(backend,/crt_bundle_attach=esp_crt_bundle_attach/);
 assert.doesNotMatch(backend,/skip_cert_common_name_check|CERTIFICATE_BUNDLE=n/);
 assert.ok(backend.indexOf('strcmp(hex,value.sha256)')<backend.indexOf('esp_ota_set_boot_partition'));
 assert.ok(backend.indexOf('strcmp(description.project_name')<backend.indexOf('esp_ota_set_boot_partition'));
 assert.match(backend,/86400000ULL/);assert.match(backend,/board::github_ota/);
 const api=fs.readFileSync(path.join(__dirname,'../src/api.cpp'),'utf8');
 assert.match(api,/updates::FlashGuard flash_guard/);assert.match(api,/if\(updates::installing\(\)\)/);
});
