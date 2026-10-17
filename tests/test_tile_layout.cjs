const {test}=require('node:test'),assert=require('node:assert/strict');
const {arrange}=require('../src/panel_web/tile_layout.js');
const {level}=require('../src/panel_web/pellet.js');
const tiles=[{memberName_:'a',seqNo_:1},{memberName_:'b',seqNo_:2},{memberName_:'c',seqNo_:3}];
test('saved tile order merges new keys and discards missing and duplicate entries',()=>{
 assert.deepEqual(arrange(tiles,['c','missing','c','a']).map(t=>t.memberName_),['c','a','b']);
 assert.deepEqual(arrange(tiles,null),tiles);
 assert.deepEqual(arrange([...tiles,tiles[0]]),tiles);
 for(const malformed of [true,{},42,'abc'])assert.deepEqual(arrange(tiles,malformed),tiles);
});
test('pellet level differentiates empty, full, invalid and stale',()=>{
 assert.equal(level(0,true),0);assert.equal(level(100,true),100);
 for(const value of [null,undefined,NaN,Infinity,-1,101,'50'])assert.equal(level(value,true),null);
 assert.equal(level(50,false),null);
});
