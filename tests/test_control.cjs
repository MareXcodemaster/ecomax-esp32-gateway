const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const source=fs.readFileSync(require.resolve('../src/panel_web/app.js'),'utf8');
const renderSource=source.slice(source.indexOf('function renderControl(){'),source.indexOf('const titles='));
function render(transport,controls,mode=0,overrides={}){
 const elements=new Map();
 const element=id=>{if(!elements.has(id))elements.set(id,{open:false,checked:false});return elements.get(id);};
 const context={state:{busy:false,write_transport:transport,curr:{mode},controls},actionBusy:false,ticket:null,controlTicket:null,
  fresh:()=>true,modeLabel:()=>String(mode),$:element,text:(id,value)=>{element(id).textContent=value;},...overrides};
 vm.runInNewContext(renderSource+'renderControl();',context);
 return {element,context};
}
test('local ecoNET enables START without asking to disconnect RS485',()=>{
 const {element}=render('econet',{start:true,stop:true,transport:'econet',mode:0});
 assert.equal(element('boiler-start').disabled,false);
 assert.equal(element('boiler-stop').disabled,true);
 assert.equal(element('control-access').textContent,'ecoNET lokalnie');
 assert.equal(element('control-transport').textContent,'ecoNET lokalnie');
});
test('hybrid uses ecoNET control and native mode keeps RS485 label',()=>{
 assert.equal(render('hybrid',{start:true,stop:true,transport:'econet'}).element('control-access').textContent,'ecoNET lokalnie');
 assert.equal(render('rs485',{start:true,stop:true,transport:'rs485'}).element('control-access').textContent,'RS485 bezpo\u015brednio');
});
test('stale state, alarm guard and running command disable START',()=>{
 assert.equal(render('econet',{start:true,stop:true,transport:'econet'},0,{fresh:()=>false}).element('boiler-start').disabled,true);
 const blocked=render('econet',{start:false,start_error:'Alarm',stop:true,transport:'econet'});
 assert.equal(blocked.element('boiler-start').disabled,true);
 assert.equal(blocked.element('control-access').textContent,'Alarm');
 assert.equal(render('econet',{start:true,stop:true,transport:'econet'},0,{actionBusy:true}).element('boiler-start').disabled,true);
});
test('running boiler permits STOP and confirmation needs explicit checkbox',()=>{
 const {element,context}=render('econet',{start:false,stop:true,transport:'econet',mode:3},3);
 assert.equal(element('boiler-stop').disabled,false);
 context.controlTicket={action:'stop',before_mode:3,transport:'econet',expires:Date.now()/1000+60};
 element('control-dialog').open=true;
 vm.runInNewContext('renderControl();',context);assert.equal(element('control-submit').disabled,true);
 element('control-checkbox').checked=true;
 vm.runInNewContext('renderControl();',context);assert.equal(element('control-submit').disabled,false);
 context.state.write_transport='rs485';
 vm.runInNewContext('renderControl();',context);assert.equal(element('control-submit').disabled,true);
});
