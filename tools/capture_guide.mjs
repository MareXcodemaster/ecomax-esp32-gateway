import { createServer } from 'vite';
import { chromium } from 'playwright';
import { mkdir,readFile } from 'node:fs/promises';

const server = await createServer({ configFile: 'installer/vite.config.js', server: { host: '127.0.0.1', port: 0 } });
await server.listen();
const browser = await chromium.launch({ headless: true });
try {
  const page = await browser.newPage({ viewport: { width: 1120, height: 820 } });
  await page.addInitScript(() => {
    let controller, configured = false, scanned = false;
    const port = {
      async open() {
        this.readable = new ReadableStream({ start(c) { controller = c; } });
        this.writable = new WritableStream({ write(bytes) {
          const cmd = JSON.parse(new TextDecoder().decode(bytes));
          if (cmd.command === 'wifi') configured = true;
          if (cmd.command === 'scan') scanned = true;
          const data = cmd.command === 'setup' ? { command: 'setup', username: 'admin', password: '0'.repeat(32) }
            : cmd.command === 'scan' ? { command: 'scan', ok: true }
            : cmd.command === 'scan_results' ? { command: 'scan_results',busy:false,result:'ESP_OK',sequence:scanned?1:0,networks:[{ssid:'Moja_siec_24GHz',rssi:-48,authmode:3}] }
            : cmd.command === 'wifi' ? { command: 'wifi', ok: true }
            : { wifi: { ssid: configured ? 'Moja_siec_24GHz' : '', connected: configured, ip: '192.0.2.10' } };
          controller.enqueue(new TextEncoder().encode('[USB] ' + JSON.stringify(data) + '\n'));
        }});
      }, async close() {}, async setSignals() {},
    };
    const serial = new EventTarget(); serial.requestPort = async () => port;
    Object.defineProperty(navigator, 'serial', { value: serial });
  });
  await mkdir('docs/images', { recursive: true });
  await page.goto(server.resolvedUrls.local[0]);
  await page.locator('#compatible').check();
  await page.screenshot({ path: 'docs/images/01-instalator.png' });
  await page.locator('#connect').click();
  await page.locator('#wifi-form').waitFor({ state: 'visible' });
  await page.locator('#scan-wifi').click();
  await page.locator('#wifi-networks option[value="Moja_siec_24GHz"]').waitFor({state:'attached'});
  await page.locator('#wifi-networks').selectOption('Moja_siec_24GHz');
  await page.locator('#wifi-password').fill('przykladowe-haslo');
  await page.locator('section[aria-labelledby="step2"]').screenshot({ path: 'docs/images/02-siec-wifi.png' });
  await page.locator('#save-wifi').click();
  await page.locator('#open-panel').waitFor({ state: 'visible' });
  await page.locator('section[aria-labelledby="step3"]').screenshot({ path: 'docs/images/03-polaczono.png' });
  const panel=await browser.newPage({viewport:{width:850,height:650}});
  const html=await readFile('src/panel_web/index.html','utf8');
  const section=html.match(/<section data-section="settings"[^>]*>[\s\S]*?<\/section>/)[0].replace(' hidden','');
  await panel.setContent('<!doctype html><html lang="pl"><meta charset="utf-8"><body><main>'+section+'</main><div id="update-notice" hidden><strong id="update-notice-version"></strong><button id="update-open"></button></div></body></html>');
  for(const name of ['style.css','updates.css'])await panel.addStyleTag({content:await readFile('src/panel_web/'+name,'utf8')});
  await panel.addScriptTag({content:await readFile('src/panel_web/updates.js','utf8')});
  await panel.addScriptTag({content:await readFile('src/panel_web/lucide.js','utf8')});
  await panel.evaluate(()=>{
    window.csrf='example';window.serverReachable=true;window.request=async()=>({current:'0.7.0-beta.4',version:'',phase:'no_stable_release',automatic:true,available:false,install_supported:true});
    EcoUpdates.init();lucide.createIcons();return EcoUpdates.load();
  });
  await panel.locator('.update-panel').screenshot({path:'docs/images/04-aktualizacje.png'});
  console.log('Created four documentation screenshots with simulated data, no physical device or private network accessed.');
} finally { await browser.close(); await server.close(); }
