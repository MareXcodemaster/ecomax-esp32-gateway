const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require.resolve('../src/panel_web/access.js'),'utf8');
const {draft}=require('../src/panel_web/access.js');
test('simple defaults and custom ASCII passwords, distinct Wi-Fi minimum',()=>{
 assert.deepEqual(draft('admin','admin','admin','',''),{current_password:'admin',password:'admin',ap_password:null,confirm:true});
 assert.equal(draft('admin','my-password','my-password','piecwifi','piecwifi').ap_password,'piecwifi');
 for(const input of [['admin','1234','1234','',''],['admin','abcde','other','',''],['admin','abcde','abcde','admin','admin'],['admin','abcde','abcde','abcdefgh','ijklmnop'],['admin','abc\nxy','abc\nxy','',''],['admin','ąbcdef','ąbcdef','',''],['admin','x'.repeat(49),'x'.repeat(49),'','']])assert.throws(()=>draft(...input));
});
function ui(fail=false,confirm=true){
 const ids=['access-current','access-password','access-repeat','access-ap','access-ap-repeat'];
 const fields=Object.fromEntries([...ids,'access-form','access-save','access-status'].map(id=>[id,{value:'',textContent:'',disabled:false}]));
 [fields[ids[0]].value,fields[ids[1]].value,fields[ids[2]].value]=['admin','my-password','my-password'];
 const calls=[],ctx={document:{getElementById:id=>fields[id]},confirm:()=>confirm,request:async(url,payload)=>{calls.push({url,payload});if(fail)throw Error('offline');return {message:'Saved'};}};
 vm.createContext(ctx);vm.runInContext(source,ctx);ctx.EcoAccess.init();
 return {fields,calls,ids,submit:()=>fields['access-form'].onsubmit({preventDefault(){}})};
}
test('password change requires confirmation, sends one request and clears fields',async()=>{
 const cancel=ui(false,false);await cancel.submit();assert.equal(cancel.calls.length,0);
 const view=ui();await view.submit();assert.equal(view.calls.length,1);assert.equal(view.calls[0].url,'/api/access');
 assert.ok(view.ids.every(id=>view.fields[id].value===''));assert.equal(view.fields['access-status'].textContent,'Saved');
});
test('unknown password-save outcome is never retried',async()=>{
 const view=ui(true);await view.submit();assert.equal(view.calls.length,1);
 assert.match(view.fields['access-status'].textContent,/Nie ponowiono/);assert.ok(view.ids.every(id=>view.fields[id].value===''));
});
test('backend protects credential writes and does not generate passwords',()=>{
 const api=fs.readFileSync(require.resolve('../src/api.cpp'),'utf8'),access=fs.readFileSync(require.resolve('../src/device_access.cpp'),'utf8');
 const handler=api.slice(api.indexOf('static esp_err_t configure_access'),api.indexOf('static esp_err_t update_status'));
 for(const guard of ['authorize(req,true)','X-Tester-Token','panel::is_busy()','FlashGuard','cJSON_IsTrue(confirm)','device_access::configure','esp_timer_start_once'])assert.ok(handler.includes(guard),guard);
 assert.doesNotMatch(access,/esp_fill_random|bootloader_random/);assert.match(access,/"admin"/);assert.match(access,/"piecwifi"/);
});
