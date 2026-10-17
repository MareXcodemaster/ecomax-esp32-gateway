const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const source=fs.readFileSync(require.resolve('../src/panel_web/index.html'),'utf8');
test('local diagnostics is a native closed disclosure containing every lower sidebar item',()=>{
 const match=source.match(/<details id="local-diagnostics"([^>]*)>([\s\S]*?)<\/details>/);
 assert.ok(match);assert.doesNotMatch(match[1],/\bopen\b/);
 assert.match(match[2],/<summary[^>]*>.*Diagnostyka lokalna.*data-lucide="chevron-right"/);
 assert.deepEqual([...match[2].matchAll(/data-view="([^"]+)"/g)].map(m=>m[1]),['data','menu','api','tests']);
 assert.match(match[2],/id="address"/);assert.doesNotMatch(match[2],/onclick=|fetch\(|\/api\//);
});
