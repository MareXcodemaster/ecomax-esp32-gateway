import { createServer } from 'vite';
import { chromium } from 'playwright';
import { mkdir } from 'node:fs/promises';

const server = await createServer({ configFile: 'installer/vite.config.js', server: { host: '127.0.0.1', port: 0 } });
await server.listen();
const browser = await chromium.launch({ headless: true });
try {
  const page = await browser.newPage({ viewport: { width: 1120, height: 820 } });
  await page.addInitScript(() => {
    let controller, configured = false;
    const port = {
      async open() {
        this.readable = new ReadableStream({ start(c) { controller = c; } });
        this.writable = new WritableStream({ write(bytes) {
          const cmd = JSON.parse(new TextDecoder().decode(bytes));
          if (cmd.command === 'wifi') configured = true;
          const data = cmd.command === 'setup' ? { command: 'setup', username: 'admin', password: '0'.repeat(32) }
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
  await page.locator('#ssid').fill('Moja_siec_24GHz');
  await page.locator('#wifi-password').fill('przykladowe-haslo');
  await page.locator('section[aria-labelledby="step2"]').screenshot({ path: 'docs/images/02-siec-wifi.png' });
  await page.locator('#save-wifi').click();
  await page.locator('#open-panel').waitFor({ state: 'visible' });
  await page.locator('section[aria-labelledby="step3"]').screenshot({ path: 'docs/images/03-polaczono.png' });
  console.log('Created three documentation screenshots with simulated USB, synthetic SSID and documentation-only IP. No physical device accessed.');
} finally { await browser.close(); await server.close(); }
