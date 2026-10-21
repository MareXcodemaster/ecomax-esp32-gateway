const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {advancedEntries,visibleTree}=require('../src/panel_web/mirror.js');
const {matchesTarget}=require('../src/panel_web/settings.js');

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
