const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {validHostname,normalizeHostname}=require('../src/panel_web/network.js');
test('hostname is one lower-case DNS label, default piec is valid',()=>{
 for(const name of ['piec','kotlownia','piec-2','a','a'.repeat(63)])assert.equal(validHostname(name),true,name);
 for(const name of ['',null,'localhost','-piec','piec-','dwa piece','żółty','PIEC','piec.local','a'.repeat(64),'x/y','x<script>'])assert.equal(validHostname(name),false,String(name));
 assert.equal(normalizeHostname('  Kotlownia.local  '),'kotlownia');
 assert.equal(normalizeHostname('PIEC.LOCAL.'),'piec');
});
test('hostname API is authenticated and UI loads the module before app startup',()=>{
 const source=fs.readFileSync(path.join(__dirname,'../src/api.cpp'),'utf8');
 const handler=source.slice(source.indexOf('static esp_err_t configure_hostname'),source.indexOf('static esp_err_t configure_sniffer'));
 assert.match(handler,/authorize\(req, true\)/);
 assert.match(handler,/network::configure_hostname/);
 assert.match(source,/add\("\/api\/hostname", HTTP_POST, configure_hostname\)/);
 const html=fs.readFileSync(path.join(__dirname,'../src/panel_web/index.html'),'utf8');
 assert.ok(html.indexOf('/network.js')<html.indexOf('/app.js'));
 assert.match(html,/id="esp-hostname-form"/);
});
