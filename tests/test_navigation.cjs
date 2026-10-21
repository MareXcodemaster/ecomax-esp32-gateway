const test=require('node:test');
const assert=require('node:assert/strict');
const {resolveView}=require('../src/panel_web/navigation.js');
const views=['live','alarms','pellet','advanced','settings','history'];

test('only known view fragments can restore a panel section',()=>{
 for(const key of views)assert.equal(resolveView('#'+key,views),key);
 assert.equal(resolveView('#%61larms',views),'alarms');
 for(const hash of ['',undefined,null,'#missing','#constructor','#__proto__','#<script>','#alarms?write=1','#%FF','#%'])assert.equal(resolveView(hash,views),'live');
});
