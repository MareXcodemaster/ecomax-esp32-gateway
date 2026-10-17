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
  pellet: { fresh: true, level_percent: 68 },
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
      : url.pathname === '/api/logout' ? { ok: true }
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
  const page = await browser.newPage({ viewport: { width: 1280, height: 1080 },hasTouch:true });
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
  assert.equal(await page.locator('#tile-layout-edit').evaluate(el=>!!el.closest('.page-toolbar')),true,'layout action does not occupy its own row');
  assert.equal(await page.locator('header').evaluate(el=>getComputedStyle(el).backgroundColor),'rgb(23, 107, 103)','green header restored');
  assert.ok((await page.locator('.eco-tile').first().boundingBox()).y<460,'desktop tiles begin without excessive blank toolbar rows');
  const screenshot = async name => {
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false, 'panel fits viewport');
    assert.deepEqual(errors, []);
    await page.screenshot({ path: `${output}/${name}.png` });
  };
  const tools=page.locator('#panel-tools'),summary=tools.locator('summary');
  const resetLayout=async()=>{await summary.click();await page.locator('#tile-layout-reset').click();assert.equal(await tools.getAttribute('open'),null,'action closes the tools menu');};
  await screenshot('01-odczyty');
  const beforeMenu=requests.filter(([method])=>method!=='GET').length;
  await summary.click();
  assert.equal(await tools.locator('.tools-list > :last-child').getAttribute('id'),'esp-logout');
  assert.equal(await page.locator('#esp-logout').innerText(),'Wyloguj');
  assert.equal(await page.locator('#scan').innerText(),'Sprawd\u017a API i katalog');
  await screenshot('09-narzedzia');
  await page.keyboard.press('Escape');assert.equal(await tools.getAttribute('open'),null);
  assert.equal(await summary.evaluate(el=>document.activeElement===el),true,'Escape restores summary focus');
  await summary.press('ArrowDown');assert.equal(await page.evaluate(()=>document.activeElement.id),'scan');
  await page.keyboard.press('End');assert.equal(await page.evaluate(()=>document.activeElement.id),'esp-logout');
  await page.keyboard.press('ArrowDown');assert.equal(await page.evaluate(()=>document.activeElement.id),'scan');
  await page.keyboard.press('Escape');
  await summary.click();await page.locator('#read-time').click();assert.equal(await tools.getAttribute('open'),null,'outside click closes menu');
  assert.equal(requests.filter(([method])=>method!=='GET').length,beforeMenu,'opening and navigating the menu never sends commands');
  const aligned=await page.locator('[data-member="fuelLevel"]').evaluate(tile=>{
    const gauge=tile.querySelector('.pellet-gauge').getBoundingClientRect(),value=tile.querySelector('.tile-value').getBoundingClientRect();
    return Math.abs(gauge.x+gauge.width/2-value.x-value.width/2)<1&&gauge.bottom<=value.top;
  });
  assert.equal(aligned,true,'pellet graphic and percent are centered on the same axis');
  const originalOrder=await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member));
  await page.locator('#tile-layout-edit').click();
  const start=await page.locator('.tile-grip').first().boundingBox(),end=await page.locator('.eco-tile').nth(2).boundingBox();
  await page.mouse.move(start.x+start.width/2,start.y+start.height/2);await page.mouse.down();
  await page.mouse.move(end.x+end.width*.75,end.y+end.height/2,{steps:20});await page.mouse.up();
  await page.waitForFunction(()=>!!localStorage.getItem('piec.tile-order.v1'));
  const customOrder=await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member));
  assert.notDeepEqual(customOrder,originalOrder,'mouse drag changes ordering');
  await screenshot('08-uklad-kafelkow');
  await page.reload();await page.locator('.eco-tile').first().waitFor();
  assert.deepEqual(await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member)),customOrder,'order survives reload and fresh data');
  await page.locator('#tile-layout-edit').click();await resetLayout();
  assert.deepEqual(await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member)),originalOrder);
  await page.locator('#tile-layout-edit').click();
  await page.evaluate(() => notice('DEMO - dane przykladowe, nie odczyt prawdziwego kotla.'));
  await page.locator('[data-view="pellet"]').click();
  assert.equal(await page.locator('#pellet-level').textContent(),'68 %');
  assert.equal(await page.locator('#pellet-hopper .pellet-fill').count(),1);
  await screenshot('07-pellet');
  for(const value of [0,100,null,101]){
    await page.evaluate(value=>EcoPellet.render({fresh:true,level_percent:value},true),value);
    assert.equal(await page.locator('#pellet-level').textContent(),value===null||value>100?'Brak odczytu':value+' %');
  }
  await page.evaluate(()=>EcoPellet.render({fresh:true,level_percent:68},false));
  assert.equal(await page.locator('#pellet-hopper').getAttribute('data-available'),'false');
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
  for(const width of [320,390]){
    await page.setViewportSize({width,height:844});await summary.click();
    const menuBox=await tools.locator('.tools-list').boundingBox();
    assert.ok(menuBox.x>=0&&menuBox.x+menuBox.width<=width&&menuBox.y+menuBox.height<=844,'tools menu fits phone viewport '+JSON.stringify({width,menuBox}));
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'named toolbar fits phone');
    await page.keyboard.press('Escape');
  }
  assert.equal(await page.locator('[data-member="fuelLevel"]').evaluate(tile=>{
    const gauge=tile.querySelector('.pellet-gauge').getBoundingClientRect(),value=tile.querySelector('.tile-value').getBoundingClientRect();
    return Math.abs(gauge.x+gauge.width/2-value.x-value.width/2)<1&&gauge.bottom<=value.top;
  }),true,'mobile pellet graphic is centered above its percent');
  await page.locator('#tile-layout-edit').click();
  const touch=await page.context().newCDPSession(page);
  const from=await page.locator('.tile-grip').first().boundingBox(),to=await page.locator('.eco-tile').nth(1).boundingBox();
  const x=from.x+from.width/2,y=from.y+from.height/2,tx=to.x+to.width*.75,ty=to.y+to.height/2;
  await touch.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y}]});
  await new Promise(resolve=>setTimeout(resolve,250));
  for(let step=1;step<=12;step++)await touch.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:x+(tx-x)*step/12,y:y+(ty-y)*step/12}]});
  await touch.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
  assert.notDeepEqual(await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member)),originalOrder,'touch drag changes order');
  await resetLayout();
  const first=page.locator('.tile-grip').first();await first.focus();await page.keyboard.press('ArrowRight');
  assert.notDeepEqual(await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member)),originalOrder,'keyboard sorting works on a narrow viewport');
  await resetLayout();
  assert.deepEqual(external, [], 'no private device or external network accessed');
  await summary.click();await page.locator('#esp-logout').click();
  await page.locator('#esp-login').waitFor({state:'visible'});
  assert.equal(await tools.getAttribute('open'),null,'logout closes tools and shows login');
  assert.deepEqual(requests.filter(([method]) => method !== 'GET'), [['POST', '/api/panel/clock'],['POST', '/api/panel/clock'],['POST','/api/logout']]);
  console.log('PASS nine panel screenshots: named tools, logout last, keyboard/Escape/outside close, no implicit commands, ordering persisted/reset, pellet empty/full/missing/stale, charts nonblank, 320/390/1280px fit.');
} finally {
  await browser.close();
  await new Promise(done => server.close(done));
}
