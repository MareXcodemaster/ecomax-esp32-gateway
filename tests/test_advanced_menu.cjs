const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {advancedEntries,visibleTree}=require('../src/panel_web/mirror.js');
const {matchesTarget}=require('../src/panel_web/settings.js');

test('service footer links to advanced menu and identifies the physical panel code without a new ESP password',()=>{
 const html=fs.readFileSync('src/panel_web/index.html','utf8');
 const service=html.slice(html.indexOf('<section data-section="service"'),html.indexOf('<section data-section="advanced"'));
 assert.equal((service.match(/data-view="advanced"/g)||[]).length,2);
 assert.ok(service.indexOf('class="parameter-actions"')>service.indexOf('id="service-tree"'));
 assert.match(service,/role="toolbar"/);
 assert.match(service,/aria-describedby="advanced-code-note"/);
 assert.match(service,/Ustawienia zaawansowane &#183; 8888/);
 assert.match(service,/nie has&#322;o logowania do ESP/);
});

test('advanced view contains only backend-approved, present, unlocked rows',()=>{
 const rows=[{index:158,advanced:true,in_structure:true,locked:false},
  {index:6,in_structure:true},{index:45,advanced:true,in_structure:false},
  {index:46,advanced:true,in_structure:true,locked:true}];
 assert.deepEqual(advancedEntries(rows),[{kind:'parameter',index:158}]);
 const tree={entries:[{kind:'group',name:'Producent',pass:2,entries:[{index:42}]}]};
 assert.equal(visibleTree(tree,false).entries.length,0);
});

test('advanced classification participates in target coherence',()=>{
 assert.equal(matchesTarget({before:1,advanced:true},{before:1,advanced:false}),false);
 assert.equal(matchesTarget({before:1,advanced:true},{before:1,advanced:true}),true);
});

test('prepare and execute separately enforce advanced confirmation before dispatch',()=>{
 const cpp=fs.readFileSync('src/panel.cpp','utf8');
 for(const [from,to]of [['J *prepare_generic(','J *execute_generic('],['J *execute_generic(','J *prepare(']]){
  const body=cpp.slice(cpp.indexOf(from),cpp.indexOf(to,cpp.indexOf(from)+1));
  assert.match(body,/advanced_menu::confirmed/);
  assert.match(body,/confirm_advanced/);
 }
 assert.match(cpp,/sections\[parent\]\.pass==3&&sections\[parent\]\.advanced&&advanced_menu::known\(wanted\)/);
 assert.match(cpp,/advanced_menu::matches\(index/);
 const source=fs.readFileSync('src/panel_web/settings.js','utf8');
 assert.match(source,/confirm_advanced:installation&&expected.advanced===true/);
 assert.doesNotMatch(source,/kasuj.*run\(|reset.*alarm.*run\(/i);
});
