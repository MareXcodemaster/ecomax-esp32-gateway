// Isolated navigation test on loopback, never a physical ESP or a boiler API.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {chromium} from 'playwright';

const source=await readFile('src/web/index.html','utf8');
const styles=[...source.matchAll(/<style>([\s\S]*?)<\/style>/g)].map(m=>m[0]).join('');
const header=source.match(/<header>[\s\S]*?<\/header>/)[0];
const login=source.match(/<dialog id="login-dialog">[\s\S]*?<\/dialog>/)[0];
const requests=[];
const server=createServer(async(req,res)=>{
 requests.push([req.method,req.url]);
 if(req.url==='/lucide.js'){res.writeHead(200,{'Content-Type':'text/javascript'}).end(await readFile('src/panel_web/lucide.js'));return;}
 res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});
 if(req.url!=='/diagnostics'){res.end('<!doctype html><h1>Panel glowny DEMO</h1>');return;}
 res.end('<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'+styles+header+'<main class="wrap" style="min-height:2200px"><h2>Diagnostyka DEMO</h2></main>'+login+'<script src="/lucide.js"></script><script>lucide.createIcons()</script></html>');
});
await new Promise(done=>server.listen(0,'127.0.0.1',done));
const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({headless:true});
await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  const page=await browser.newPage({viewport:{width,height:844}}),errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.goto(origin+'/diagnostics');
  const back=page.getByRole('link',{name:'Wr\u00f3\u0107 do panelu',exact:true});
  assert.equal(await back.count(),1,'only visible header link, not the closed dialog');
  assert.equal(await back.getAttribute('href'),'/');
  assert.equal(await page.locator('#panel-back svg.lucide-arrow-left').count(),1);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'header fits viewport');
  assert.equal(await back.evaluate(el=>el.scrollWidth>el.clientWidth+1),false,'label fits button');
  await page.evaluate(()=>scrollTo(0,1000));
  assert.ok((await back.boundingBox()).y>=0,'return link remains visible after scrolling');
  await page.screenshot({path:`test-results/diagnostics-back-${width}.png`});
  await page.locator('header').screenshot({path:`test-results/diagnostics-header-${width}.png`});
  await back.focus();
  await back.press('Enter');await page.waitForURL(origin+'/');
  assert.equal(await page.getByRole('heading',{name:'Panel glowny DEMO'}).count(),1,'keyboard navigation reaches same-origin root');
  await page.goto(origin+'/diagnostics');await page.evaluate(()=>document.getElementById('login-dialog').showModal());
  await page.locator('#login-dialog').getByRole('link',{name:'Wr\u00f3\u0107 do panelu',exact:true}).click();
  await page.waitForURL(origin+'/');
  assert.deepEqual(errors,[]);await page.close();
  console.log(`PASS diagnostics return ${width}px: icon, sticky header, keyboard, login dialog, same-origin root`);
 }
 assert.ok(requests.every(([method,url])=>method==='GET'&&!url.startsWith('/api/')),'return sends no commands and needs no API');
}finally{await browser.close();await new Promise(done=>server.close(done));}
