import { chromium } from 'playwright';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import assert from 'node:assert/strict';

const root = resolve('site');
const server = createServer(async (req, res) => {
  const path = resolve(root, '.' + decodeURIComponent(new URL(req.url, 'http://localhost').pathname));
  if (path !== root && !path.startsWith(root + sep)) { res.writeHead(403).end(); return; }
  try {
    const file = path === root ? resolve(root, 'index.html') : path;
    const data = await readFile(file);
    const types = { '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.html': 'text/html', '.png': 'image/png', '.svg': 'image/svg+xml' };
    res.writeHead(200, { 'Content-Type': types[extname(file)] || 'application/octet-stream' }).end(data);
  } catch { res.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
const browser = await chromium.launch({ headless: true });
await mkdir('test-results', { recursive: true });
try {
  for (const width of [1280, 390]) {
    const page = await browser.newPage({ viewport: { width, height: 900 } });
    const errors = [], external = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.route('**/*', async route => {
      if (!route.request().url().startsWith(origin + '/')) { external.push(route.request().url()); await route.abort(); }
      else await route.continue();
    });
    await page.addInitScript(() => {
      window.usbCommands = [];
      let controller;
      const encoder = new TextEncoder();
      const mock = {
        async open() {
          this.readable = new ReadableStream({ start(c) { controller = c; } });
          this.writable = new WritableStream({ write(bytes) {
            const command = JSON.parse(new TextDecoder().decode(bytes));
            window.usbCommands.push(command);
            const response = command.command === 'setup'
              ? { command: 'setup', username: 'admin', password: '0123456789abcdef'.repeat(2) }
              : command.command === 'wifi' ? { command: 'wifi', ok: true }
              : { wifi: { ssid: window.usbCommands.find(x => x.command === 'wifi')?.ssid || '',
                  connected: window.usbCommands.some(x => x.command === 'wifi'), ip: '192.0.2.10' } };
            controller.enqueue(encoder.encode('[USB] ' + JSON.stringify(response) + '\n'));
          }});
        },
        async close() {}, async setSignals() {},
      };
      const serial = new EventTarget(); serial.requestPort = async () => mock;
      Object.defineProperty(navigator, 'serial', { configurable: true, value: serial });
    });
    await page.goto(origin);
    await page.locator('#connect').waitFor({ state: 'visible' });
    assert.equal(await page.locator('#install').isEnabled(), false);
    await page.locator('#compatible').check();
    assert.equal(await page.locator('#install').isEnabled(), true);
    assert.ok(await page.locator('.lucide').count() > 5, 'local icons rendered');
    await page.locator('#connect').click();
    await page.locator('#wifi-form').waitFor({ state: 'visible' });
    assert.equal(await page.locator('#device-password').getAttribute('type'), 'password');
    assert.equal(await page.locator('#install').isEnabled(), false, 'serial is owned by setup, not flasher');
    await page.locator('#ssid').fill('Example network');
    await page.locator('#wifi-password').fill('example-pass-123');
    await page.locator('#save-wifi').click();
    await page.locator('#open-panel').waitFor({ state: 'visible' });
    assert.equal(await page.locator('#open-panel').getAttribute('href'), 'http://192.0.2.10/');
    assert.equal(await page.locator('#wifi-password').inputValue(), '');
    assert.equal(await page.evaluate(() => window.usbCommands.filter(x => x.command === 'wifi').length), 1);
    assert.deepEqual(await page.evaluate(() => [localStorage.length, sessionStorage.length]), [0, 0]);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, 'no horizontal overflow');
    for (const item of await page.locator('button:visible').all()) {
      assert.equal(await item.evaluate(el => el.scrollWidth > el.clientWidth + 1), false, 'button text fits');
    }
    await page.screenshot({ path: `test-results/installer-${width}.png`, fullPage: true });
    await page.locator('#disconnect').click();
    assert.equal(await page.locator('#credentials').isVisible(), false);
    assert.equal(await page.locator('#device-password').inputValue(), '');
    assert.deepEqual(errors, []);
    assert.deepEqual(external, [], 'no third-party script, telemetry or credentials request');
    await page.close();
    console.log(`PASS installer ${width}px: USB provisioning, one write, no browser storage or external requests, layout`);
  }
  const unsupported = await browser.newPage();
  await unsupported.addInitScript(() => Object.defineProperty(navigator, 'serial', { value: undefined }));
  await unsupported.goto(origin);
  await unsupported.locator('#unsupported').waitFor({ state: 'visible' });
  assert.equal(await unsupported.locator('#connect').isEnabled(), false);
  console.log('PASS unsupported browser: clear fallback, USB actions disabled');
  await unsupported.close();
  for (const width of [1280,390]) {
    const page=await browser.newPage({viewport:{width,height:900}});
    await page.route('https://www.waveshare.com/**',route=>route.abort());
    await page.goto(origin+'/instrukcja.html');
    assert.match(await page.title(),/ecoMAX/);
    assert.equal(await page.locator('h1').count(),1);
    await page.waitForFunction(()=>[...document.images].filter(img=>img.src.startsWith(location.origin)).every(img=>img.complete&&img.naturalWidth>0));
    assert.equal(await page.locator('img').count(),6);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'guide fits screen');
    await page.screenshot({path:`test-results/guide-${width}.png`,fullPage:true});
    await page.close();
    console.log(`PASS Polish guide ${width}px: local screenshots and wiring image loaded, layout`);
  }
  const html=await readFile('src/panel_web/index.html','utf8');
  const section=html.match(/<section data-section="espwifi"[^>]*>[\s\S]*?<\/section>/)[0].replace(' hidden','');
  for (const width of [1280,390]) {
    const page=await browser.newPage({viewport:{width,height:850}});
    const errors=[];page.on('pageerror',error=>errors.push(error.message));
    await page.setContent('<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><body><main>'+section+'<div id="address"></div></main></body></html>');
    for(const file of ['style.css','network.css'])await page.addStyleTag({content:await readFile('src/panel_web/'+file,'utf8')});
    await page.addScriptTag({content:await readFile('src/panel_web/network.js','utf8')});
    await page.addScriptTag({content:await readFile('src/panel_web/lucide.js','utf8')});
    await page.evaluate(()=>{
      window.state={write_transport:'rs485'};window.hostnameWrites=[];
      window.request=async(path,payload)=>{window.hostnameWrites.push({path,payload});return {local_url:'http://'+payload.hostname+'.local/'};};
      EcoNetwork.init();EcoNetwork.render({hostname:'piec'});lucide.createIcons();
    });
    assert.equal(await page.locator('#esp-hostname').inputValue(),'piec');
    await page.locator('#esp-hostname').fill('bad name');
    await page.locator('#esp-hostname-save').click();
    assert.equal(await page.evaluate(()=>window.hostnameWrites.length),0);
    await page.locator('#esp-hostname').fill('Kotlownia.local');
    await page.evaluate(()=>EcoNetwork.render({hostname:'piec'}));
    assert.equal(await page.locator('#esp-hostname').inputValue(),'Kotlownia.local','poll preserves draft');
    await page.locator('#esp-hostname-save').click();
    await page.locator('#esp-hostname-link').waitFor({state:'visible'});
    assert.equal(await page.locator('#esp-hostname-link').getAttribute('href'),'http://kotlownia.local/');
    assert.deepEqual(await page.evaluate(()=>window.hostnameWrites),[{path:'/api/hostname',payload:{hostname:'kotlownia'}}]);
    await page.evaluate(()=>EcoNetwork.render({hostname:'kotlownia'}));
    assert.equal(await page.locator('#address').innerText(),'kotlownia.local / RS485');
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
    await page.screenshot({path:`test-results/network-${width}.png`,fullPage:true});
    assert.deepEqual(errors,[]);await page.close();
    console.log(`PASS hostname ${width}px: validation, preserved draft, one save, new local address`);
  }
  const updateSection=html.match(/<section data-section="settings"[^>]*>[\s\S]*?<\/section>/)[0].replace(' hidden','');
  for(const width of [1280,390]){
    const page=await browser.newPage({viewport:{width,height:850}});
    const errors=[];page.on('pageerror',error=>errors.push(error.message));
    await page.setContent('<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><body><main>'+updateSection+'</main><div id="update-notice"><strong id="update-notice-version"></strong><button id="update-open">Aktualizacja</button></div></body></html>');
    for(const file of ['style.css','updates.css'])await page.addStyleTag({content:await readFile('src/panel_web/'+file,'utf8')});
    await page.addScriptTag({content:await readFile('src/panel_web/updates.js','utf8')});
    await page.addScriptTag({content:await readFile('src/panel_web/lucide.js','utf8')});
    await page.evaluate(()=>{
      window.csrf='example';window.serverReachable=true;window.updateRequests=[];
      window.updateState={current:'0.7.0-beta.3',version:'0.7.0',available:true,install_supported:true,automatic:true,phase:'available',busy:false};
      window.request=async(url,payload)=>{
        window.updateRequests.push({url,payload});
        if(url==='/api/updates/config')window.updateState.automatic=payload.automatic;
        if(url==='/api/updates/install')window.updateState={...window.updateState,busy:true,phase:'downloading',received:1024,size:200000};
        return window.updateState;
      };
      EcoUpdates.init();lucide.createIcons();return EcoUpdates.load();
    });
    assert.equal(await page.evaluate(()=>window.updateRequests.filter(r=>r.payload).length),0);
    assert.equal(await page.locator('#update-install').isEnabled(),true);
    await page.locator('#update-auto').uncheck();
    await page.waitForFunction(()=>window.updateState.automatic===false);
    await page.screenshot({path:`test-results/updates-${width}.png`,fullPage:true});
    page.once('dialog',dialog=>dialog.accept());
    await page.locator('#update-install').click();
    await page.waitForFunction(()=>window.updateState.phase==='downloading');
    await page.evaluate(async()=>{await EcoUpdates.load();await EcoUpdates.load();});
    assert.equal(await page.evaluate(()=>window.updateRequests.filter(r=>r.url==='/api/updates/install').length),1);
    assert.equal(await page.locator('#update-install').isEnabled(),false);
    assert.equal(await page.locator('#update-progress').isVisible(),true);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
    assert.deepEqual(errors,[]);await page.close();
    console.log(`PASS stable updater ${width}px: no unattended install, opt-out, explicit confirmation, one POST, progress and layout`);
  }
} finally {
  await browser.close();
  await new Promise(resolve => server.close(resolve));
}
