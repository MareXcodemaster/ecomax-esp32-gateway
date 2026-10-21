const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');

for(const file of ['src/panel_web/index.html','src/web/index.html']){
 test(file+': local SVG, ICO and touch icon are shared without credentials',()=>{
  const html=fs.readFileSync(file,'utf8');
  assert.match(html,/<link rel="icon" href="\/favicon\.ico" sizes="32x32">/);
  assert.match(html,/<link rel="icon" type="image\/svg\+xml" href="\/assets\/favicon\.svg">/);
  assert.match(html,/<link rel="apple-touch-icon" href="\/assets\/icon-180\.png">/);
 });
}
test('green flame artwork has no external dependencies and raster fallbacks have correct dimensions',()=>{
 const svg=fs.readFileSync('src/panel_web/assets/favicon.svg','utf8');
 assert.match(svg,/<rect[^>]+fill="#176b67"/);
 assert.match(svg,/fill="#fff"/);
 assert.doesNotMatch(svg,/<script|<image|href=|foreignObject/);
 for(const size of [16,32,180]){
  const png=fs.readFileSync(`src/panel_web/assets/icon-${size}.png`);
  assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
  assert.equal(png.readUInt32BE(16),size);assert.equal(png.readUInt32BE(20),size);
 }
});
test('ICO contains the 32px PNG and firmware supplies explicit image MIME types',()=>{
 const ico=fs.readFileSync('src/panel_web/favicon.ico');
 assert.equal(ico.readUInt16LE(0),0);assert.equal(ico.readUInt16LE(2),1);assert.equal(ico.readUInt16LE(4),1);
 assert.equal(ico[6],32);assert.equal(ico[7],32);assert.equal(ico.readUInt16LE(12),32);
 assert.equal(ico.readUInt32LE(14),ico.length-22);assert.equal(ico.readUInt32LE(18),22);
 assert.deepEqual(ico.subarray(22),fs.readFileSync('src/panel_web/assets/icon-32.png'));
 const embed=fs.readFileSync('embed_web.py','utf8');
 assert.match(embed,/"\.ico": "image\/x-icon"/);assert.match(embed,/"\.png": "image\/png"/);
});
