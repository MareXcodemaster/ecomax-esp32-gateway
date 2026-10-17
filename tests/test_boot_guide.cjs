const test=require('node:test');
const assert=require('node:assert/strict');
const {readFileSync}=require('node:fs');
const {join}=require('node:path');
const root=join(__dirname,'..');
const read=name=>readFileSync(join(root,name),'utf8');

test('USB recovery has a local illustration in every installation guide',()=>{
 for(const name of ['SZYBKI_START','START_PL','INSTALACJA']){
  const text=read('docs/'+name+'.md');
  assert.match(text,/images\/boot-usb\.svg/);
  assert.match(text,/2-3 sekund/);
  assert.match(text,/BOOT.*nie RESET/);
  assert.match(text,/Firmware-Flashing/);
 }
 const svg=read('docs/images/boot-usb.svg');
 assert.match(svg,/BOOT = 13, RESET = 14/);
 assert.match(svg,/Rysunek pogladowy/);
 assert.doesNotMatch(svg,/<script|<foreignObject|\son\w+=|href=/i,'diagram is static and self-contained');
});

test('BOOT hardware reference is attributed and does not claim a new board test',()=>{
 const hardware=read('docs/SPRZET.md');
 assert.match(hardware,/BOOT to element 13/);
 assert.match(hardware,/docs\.waveshare\.net\/assets\/images\//);
 assert.match(hardware,/nie jest objete MIT/);
 assert.match(read('README.md'),/To nie jest test drugiego egzemplarza/);
 assert.match(read('docs/START_PL.md'),/Nie jest to test drugiego egzemplarza/);
});
