const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs');
const {validate,reasons}=require('../src/panel_web/compatibility_report.js');
test('compatibility download requires matching read-only report, not an ordinary state export',()=>{
 const report={format:'ecomax-compatibility',schema:1,read_only:true,report_id:'demo',read_results:[],warnings:[],partial:true};
 assert.equal(validate(report,'demo'),report);
 for(const patch of [{format:'state'},{schema:2},{read_only:false},{report_id:'old'},{read_results:null},{warnings:null}])assert.throws(()=>validate({...report,...patch},'demo'));
 assert.throws(()=>validate(null,'demo'));
 assert.match(reasons.unsupported_model,/model/);assert.match(reasons.identity_mismatch,/inny kocioł/);
});
test('compatibility task uses independent fixed reads without enrollment, mode changes or boiler writes',()=>{
 const source=fs.readFileSync('src/panel.cpp','utf8'),body=source.slice(source.indexOf('J *collect_compatibility('),source.indexOf('void worker('));
 assert.match(body,/transport::request_readonly/);
 assert.doesNotMatch(body,/remember_identity\(|transport::(?:request\(|enable\(|disable\()|rs_write|execute_control|put\(report,"(?:system|menu|names)"/);
 const api=fs.readFileSync('src/api.cpp','utf8');assert.match(api,/add\("\/api\/compatibility",HTTP_GET,compatibility_state\)/);
 assert.match(api,/add\("\/api\/compatibility\/scan",HTTP_POST,panel_command\)/);
 const handler=api.slice(api.indexOf('static esp_err_t compatibility_state'),api.indexOf('static esp_err_t pellet_state'));
 assert.match(handler,/authorize\(req\)/);assert.match(handler,/404 Not Found/);
});
