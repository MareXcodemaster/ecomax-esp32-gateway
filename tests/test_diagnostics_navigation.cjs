const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const html=fs.readFileSync(require('node:path').join(__dirname,'../src/web/index.html'),'utf8');
test('diagnostics returns to same ESP through a real link, not browser history or a hardcoded host',()=>{
 for(const id of ['panel-back','login-panel-back']){
  assert.match(html,new RegExp('<a id="'+id+'"[^>]*href="/"'));
 }
 assert.match(html,/data-lucide="arrow-left"/);
 assert.match(html,/<script src="\/lucide.js"><\/script>/);
 assert.doesNotMatch(html,/history\.back\(|id="panel-back"[^>]*(?:onclick|target)=/);
});
