const test=require('node:test'),assert=require('node:assert/strict');
const {weekOrder,paintWeekRange,validSchedule}=require('../src/panel_web/settings.js');
const {decodeSchedule}=require('../src/panel_web/mirror.js');
const week=()=>[...Array.from({length:7},()=>[0,0,0,0,0,0]),[1,3,3,3]];
test('weekly presentation starts Monday, but the wire retains Sunday at index zero',()=>{
 assert.deepEqual(weekOrder,[1,2,3,4,5,6,0]);
 for(let row=0;row<7;row++)for(let slot=0;slot<48;slot++){
  const original=week(),painted=paintWeekRange(original,[row,slot],[row,slot],true);
  assert.equal(validSchedule(painted),true);assert.deepEqual(original,week());
  for(let day=0;day<7;day++)assert.deepEqual(decodeSchedule(painted,day).slots,Array.from({length:48},(_,i)=>day===weekOrder[row]&&i===slot));
  assert.deepEqual(painted[7],original[7]);
 }
});
test('rectangular edits fill half hours across weekdays without modifying the weekend or metadata',()=>{
 const original=week(),painted=paintWeekRange(original,[0,12],[4,19],true);
 for(let row=0;row<7;row++)assert.deepEqual(decodeSchedule(painted,weekOrder[row]).slots,Array.from({length:48},(_,i)=>row<5&&i>=12&&i<=19));
 assert.deepEqual(painted[7],[1,3,3,3]);
 assert.deepEqual(paintWeekRange(painted,[4,19],[0,12],false),original);
});
test('an overnight transition never moves Sunday into Monday or changes the enable flag',()=>{
 const original=week();original[7][0]=0;
 const painted=paintWeekRange(original,[6,46],[6,47],true);
 assert.deepEqual(painted[0],[0,0,0,0,0,3]);assert.deepEqual(painted[1],original[1]);assert.deepEqual(painted[7],original[7]);
});
test('bad points and malformed weeks cannot produce a draft',()=>{
 for(const point of [[-1,0],[7,0],[0,-1],[0,48],[0,.5],null])assert.throws(()=>paintWeekRange(week(),point,[0,0],true));
 assert.throws(()=>paintWeekRange([], [0,0],[0,0],true));assert.throws(()=>paintWeekRange(week(),[0,0],[0,0],1));
});
