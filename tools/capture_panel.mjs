// Render the real panel with synthetic data on loopback, never a physical boiler.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright';

const root = resolve('src/panel_web'), now = Math.floor(Date.now() / 1000);
const version = JSON.parse(await readFile('package.json', 'utf8')).version;
const week = Array.from({ length: 7 }, (_, day) => {
  const slots = Array.from({ length: 48 }, (_, slot) =>
    day === 0 || day === 6 ? slot >= 14 && slot < 44 :
      slot >= 12 && slot < 16 || slot >= 32 && slot < 44);
  return Array.from({ length: 6 }, (_, byte) =>
    slots.slice(byte * 8, byte * 8 + 8).reduce((v, on, i) => v | (on ? 1 << (7 - i) : 0), 0));
});
week.push([1, 5, 0, 20]);
const target = (name, before, min, max, measured_key, step = 1) =>
  ({ name, before, effective: before, min, max, step, measured_key, write_ready: true });
const state = {
  version, csrf: 'documentation-demo', fresh: true, connected: true, busy: false,
  error: '', last_read_utc: new Date(now * 1000).toISOString(), age_seconds: 0.5,
  write_transport: 'rs485', targets_fresh: true, host: '', values: {}, names: {},
  menu: [], enums: {}, units: [], structure: [], category_names: [], tiles: [],
  endpoints: {}, versions: {}, requires_configuration: false,
  system: { controllerID: 'ecoMAX (DEMO)', moduleASoftVer: 'przyklad', modulePanelSoftVer: 'przyklad' },
  controls: { start: false, stop: true, transport: 'rs485', mode: 3 },
  curr: { mode: 3, tempCO: 72.5, tempCOSet: 75, tempCWU: 43.6, tempCWUSet: 45,
    tempExternalSensor: 8.4, tempFlueGas: 112.7, tempFeeder: 26.3,
    mixerTemp1: 39.2, mixerSetTemp1: 40, ecoSterTemp1: 21.8, ecoSterSetTemp1: 22,
    ecoSterMode1: 0, fuelLevel: 68, boilerPower: 45, fanPower: 32,
    lambdaLevel: 105, lambdaStatus: 1, pumpCOWorks: true, pumpCWUWorks: false,
    mixerPumpWorks1: true, fanWorks: true, feederWorks: false, lighterWorks: false },
  targets: {
    1280: target('Temperatura zadana kotla', 75, 70, 85, 'tempCO'),
    1281: target('Temperatura zadana CWU', 45, 35, 65, 'tempCWU'),
    1287: target('Temperatura mieszacza 1', 40, 20, 70, 'mixerTemp1'),
    STER_TEMP_DAY_1: target('Temperatura pokojowa', 22, 10, 30, 'ecoSterTemp1', 0.1),
  },
  econet_health: { fresh: true, available: false, api_ok: false },
  esp_network: { connected: true, ssid: 'Moja_siec_24GHz', ip: '192.0.2.10',
    rssi_dbm: -49, channel: 6, mdns: true, hostname: 'piec', local_url: 'http://piec.local/' },
  schedules: { ecomaxSchedules: { cwuTZ: week, thermostat1TZ: week } },
  schedule_status: { fresh: true, read_epoch: now },
  thermostat_schedule_status: { fresh: true, read_epoch: now },
  schedule_write_indices: { cwuTZ: 0, thermostat1TZ: 1 },
};
const columns = ['time', 'native', 'mode', 'co', 'coSet', 'cwu', 'cwuSet', 'room', 'roomSet'];
const rows = Array.from({ length: 433 }, (_, i) => {
  const phase = i % 72, mode = phase < 4 ? 2 : phase < 8 ? 1 : phase < 50 ? 3 : phase < 56 ? 7 : 6;
  return [now - (432 - i) * 600, 1, mode,
    +(68 + 6 * Math.sin(i / 10)).toFixed(1), 75,
    +(42 + 2 * Math.sin(i / 18)).toFixed(1), 45,
    +(21.5 + 0.4 * Math.sin(i / 22)).toFixed(1), 22];
});
const history = { schema: 1, columns, rows, gap_seconds: 1200, resolution_seconds: 600,
  archive: { ready: true, samples: 4320, oldest_epoch: now - 3 * 86400, newest_epoch: now } };
const requests = [], errors = [], external = [];
const server = createServer(async (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  if (url.pathname.startsWith('/api/')) {
    requests.push([req.method, url.pathname]);
    const data = url.pathname === '/api/session' ? { csrf: state.csrf }
      : url.pathname === '/api/state' ? state
      : url.pathname === '/api/panel/clock' ? { ok: true }
      : url.pathname === '/api/history' ? history
      : url.pathname === '/api/updates' ? { current: version, phase: 'no_stable_release', automatic: true, available: false, install_supported: true }
      : null;
    res.writeHead(data ? 200 : 404, { 'Content-Type': 'application/json' }).end(JSON.stringify(data || { error: 'Demo: endpoint disabled' }));
    return;
  }
  const path = resolve(root, '.' + decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname));
  if (!path.startsWith(root + sep)) { res.writeHead(403).end(); return; }
  try {
    const types = { '.js': 'text/javascript', '.css': 'text/css', '.html': 'text/html', '.svg': 'image/svg+xml', '.woff2': 'font/woff2' };
    res.writeHead(200, { 'Content-Type': types[extname(path)] || 'application/octet-stream' }).end(await readFile(path));
  } catch { res.writeHead(404).end(); }
});
await new Promise(done => server.listen(0, '127.0.0.1', done));
const origin = `http://127.0.0.1:${server.address().port}`;
const browser = await chromium.launch({ headless: true });
const output = 'docs/images/panel';
await mkdir(output, { recursive: true });
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1080 } });
  page.on('pageerror', error => errors.push(error.message));
  await page.route('**/*', async route => {
    if (!route.request().url().startsWith(origin + '/')) { external.push(route.request().url()); await route.abort(); }
    else await route.continue();
  });
  await page.goto(origin);
  await page.locator('.eco-tile .tile-value').first().waitFor();
  await page.evaluate(() => notice('DEMO - dane przykladowe, nie odczyt prawdziwego kotla.'));
  await page.evaluate(() => document.fonts.ready);
  const targetLabels = await page.locator('.tile-target-now').evaluateAll(elements => elements.map(el => {
    const actual = el.closest('.tile-body').querySelector('[data-reading="actual"]').getBoundingClientRect();
    const label = el.getBoundingClientRect();
    return !el.closest('.tile-symbol') && label.bottom <= actual.top + 1;
  }));
  assert.deepEqual(targetLabels, [true, true, true], 'target labels stay with the setpoint and do not overlap measured temperature');
  const screenshot = async name => {
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, 'panel fits viewport');
    assert.deepEqual(errors, []);
    await page.screenshot({ path: `${output}/${name}.png` });
  };
  await screenshot('01-odczyty');
  await page.locator('[data-view="schedule"]').click();
  assert.equal(await page.locator('.week-cell').count(), 336);
  assert.equal(await page.locator('.week-cell:not(:disabled)').count(), 336);
  await screenshot('02-harmonogram');
  await page.locator('[data-view="history"]').click();
  await page.waitForFunction(() => document.getElementById('history-range').textContent.includes('433'));
  const canvas = await page.locator('#chart').evaluate(el => {
    const p = el.getContext('2d').getImageData(0, 0, el.width, el.height).data;
    let colored = 0;
    for (let i = 0; i < p.length; i += 4) if (p[i + 3] && Math.max(p[i], p[i + 1], p[i + 2]) - Math.min(p[i], p[i + 1], p[i + 2]) > 30) colored++;
    return colored;
  });
  assert.ok(canvas > 1000, 'temperature graph has rendered colored data');
  await screenshot('03-historia');
  await page.locator('#history-source').selectOption('states');
  await screenshot('04-stany-kotla');
  await page.locator('[data-view="live"]').click();
  await page.locator('[data-member="tempCWU"] .tile-action').click();
  await page.locator('#temperature-dialog').waitFor({ state: 'visible' });
  await screenshot('05-nastawa');
  await page.locator('#temperature-cancel').click();
  await page.setViewportSize({ width: 390, height: 844 });
  await screenshot('06-telefon');
  assert.deepEqual(external, [], 'no private device or external network accessed');
  assert.deepEqual(requests.filter(([method]) => method !== 'GET'), [['POST', '/api/panel/clock']]);
  console.log('PASS six panel screenshots: real UI, synthetic data, no device access or boiler writes, charts nonblank, desktop/mobile fit.');
} finally {
  await browser.close();
  await new Promise(done => server.close(done));
}
