// Raster fallbacks come from the same local SVG, without external services.
import {readFile,writeFile} from 'node:fs/promises';
import {chromium} from 'playwright';

const svg=await readFile('src/panel_web/assets/favicon.svg','utf8');
const browser=await chromium.launch({headless:true});
try{
 const page=await browser.newPage({deviceScaleFactor:1});
 for(const size of [16,32,180]){
  await page.setViewportSize({width:size,height:size});
  await page.setContent('<!doctype html><style>html,body{margin:0;width:100%;height:100%;background:transparent}svg{display:block;width:100%;height:100%}</style>'+svg);
  const png=await page.screenshot({omitBackground:true});
  await writeFile(`src/panel_web/assets/icon-${size}.png`,png);
  if(size===32){
   const header=Buffer.alloc(22);
   header.writeUInt16LE(1,2);header.writeUInt16LE(1,4);
   header[6]=size;header[7]=size;header.writeUInt16LE(1,10);header.writeUInt16LE(32,12);
   header.writeUInt32LE(png.length,14);header.writeUInt32LE(header.length,18);
   await writeFile('src/panel_web/favicon.ico',Buffer.concat([header,png]));
  }
 }
 console.log('Generated local favicon fallbacks: 16/32px PNG, ICO and 180px touch icon.');
}finally{await browser.close();}
