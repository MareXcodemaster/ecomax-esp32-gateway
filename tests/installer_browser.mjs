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
    const types = { '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.html': 'text/html' };
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
} finally {
  await browser.close();
  await new Promise(resolve => server.close(resolve));
}
