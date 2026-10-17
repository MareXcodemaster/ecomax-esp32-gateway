const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../src/web/index.html'),'utf8');
test('frame inspector keeps partial recognition distinct and writes no device commands',()=>{
 const code=source.match(/let lastFrameStats=0[\s\S]*?(?=\$\('export'\)\.onclick)/)[0];
 assert.match(code,/partial:/);assert.match(code,/selectedFrame=null/);assert.match(code,/selection!==frameSelection/);
 assert.match(code,/textContent/);assert.doesNotMatch(code,/innerHTML|jsonPost|method:\s*['"]POST|\/api\/sniffer/);
 assert.match(source,/<div id="frame-decoded-values"/);assert.match(source,/Dane techniczne i HEX/);
});
