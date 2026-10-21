// Render the real panel with synthetic data on loopback, never a physical boiler.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright';

const root = resolve('src/panel_web'), now = Math.floor(Date.now() / 1000);
const version = JSON.parse(await readFile('package.json', 'utf8')).version;
const deviceInfo = JSON.parse(await readFile('tests/fixtures/device_info.json','utf8'));
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
  system: { controllerID: 'ecoMAX (DEMO)', schemaID:23, moduleASoftVer: 'przyklad', modulePanelSoftVer: 'przyklad' },
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
  esp_device: deviceInfo,
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
      : url.pathname === '/api/updates' ? { current: version, current_release:version,version,channel:'all',beta:true,phase:'up_to_date',checked:now, automatic: true, available: false, install_supported: true }
      : null;
    res.writeHead(data ? 200 : 404, { 'Content-Type': 'application/json' }).end(JSON.stringify(data || { error: 'Demo: endpoint disabled' }));
    return;
  }
  const path = resolve(root, '.' + decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname));
  if (!path.startsWith(root + sep)) { res.writeHead(403).end(); return; }
  try {
    const types = { '.js': 'text/javascript', '.css': 'text/css', '.html': 'text/html', '.svg': 'image/svg+xml', '.jpg':'image/jpeg', '.woff2': 'font/woff2' };
    res.writeHead(200, { 'Content-Type': types[extname(path)] || 'application/octet-stream' }).end(await readFile(path));
  } catch { res.writeHead(404).end(); }
});
await new Promise(done => server.listen(0, '127.0.0.1', done));
const origin = `http://127.0.0.1:${server.address().port}`;
const browser = await chromium.launch({ headless: true });
const output = `docs/images/panel/${version}`;
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
  assert.equal(await page.locator('#update-open').evaluate(el=>!!el.closest('.page-toolbar')),true,'update action shares the toolbar, never a separate band');
  assert.equal(await page.locator('header #connect-open').count(),0);
  assert.equal(await page.locator('#panel-tools #connect-open').count(),1);
  assert.equal(await page.locator('[data-view="pellet"] svg.lucide-cylinder').count(),1,'pellet navigation uses a cylindrical pellet symbol');
  await page.locator('#update-open-label').getByText('Aktualne',{exact:true}).waitFor();
  await page.locator('#wifi-signal').getByText('100%',{exact:true}).waitFor();
  assert.equal(await page.locator('.wifi-bars span').count(),5);
  assert.equal(await page.locator('.wifi-bars .active').count(),5);
  assert.match(await page.locator('#wifi-signal').getAttribute('title'),/-49 dBm/);
  assert.equal(await page.locator('#local-diagnostics').getAttribute('open'),null);
  const resetLayout=async()=>{await summary.click();await page.locator('#tile-layout-reset').click();assert.equal(await tools.getAttribute('open'),null,'action closes the tools menu');};
  await screenshot('01-odczyty');
  const beforeMenu=requests.filter(([method])=>method!=='GET').length;
  state.alarm_status={count_fresh:true,pending_count:3,count_age_seconds:1};
  for(const width of [320,390,1280]){
    await page.setViewportSize({width,height:1080});
    await page.evaluate(()=>EcoAlarmBadge.render({write_transport:'rs485',alarm_status:{count_fresh:true,pending_count:3,count_age_seconds:1},alarms:[{active:false}]},true));
    const counter=page.locator('#alarm-count');assert.equal(await counter.textContent(),'3');
    const style=await counter.evaluate(el=>{const s=getComputedStyle(el);return {display:s.display,color:s.backgroundColor,radius:s.borderRadius,w:el.getBoundingClientRect().width,h:el.getBoundingClientRect().height};});
    assert.equal(style.color,'rgb(198, 40, 40)');assert.equal(style.radius,'50%');assert.ok(Math.abs(style.w-22)<.01);assert.ok(Math.abs(style.w-style.h)<.01);assert.notEqual(style.display,'none');
  }
  state.alarm_status={count_fresh:true,pending_count:0};
  await page.setViewportSize({width:1280,height:1080});
  await page.evaluate(()=>EcoAlarmBadge.render({write_transport:'rs485',alarm_status:{count_fresh:true,pending_count:0}},true));
  assert.equal(await page.locator('#alarm-count').isVisible(),false);
  await page.evaluate(()=>EcoAlarmBadge.render({write_transport:'rs485',alarm_status:{count_fresh:true,pending_count:3}},false));
  assert.equal(await page.locator('#alarm-count').isVisible(),false);
  await summary.click();
  assert.equal(await tools.locator('.tools-list > :last-child').getAttribute('id'),'esp-logout');
  assert.equal(await page.locator('#esp-logout').innerText(),'Wyloguj');
  assert.equal(await page.locator('#scan').innerText(),'Sprawd\u017a API i katalog');
  await screenshot('09-narzedzia');
  await page.keyboard.press('Escape');assert.equal(await tools.getAttribute('open'),null);
  assert.equal(await summary.evaluate(el=>document.activeElement===el),true,'Escape restores summary focus');
  await summary.press('ArrowDown');assert.equal(await page.evaluate(()=>document.activeElement.id),'wifi-open');
  await page.keyboard.press('End');assert.equal(await page.evaluate(()=>document.activeElement.id),'esp-logout');
  await page.keyboard.press('ArrowDown');assert.equal(await page.evaluate(()=>document.activeElement.id),'wifi-open');
  await page.keyboard.press('Escape');
  await summary.click();await page.locator('#read-time').click();assert.equal(await tools.getAttribute('open'),null,'outside click closes menu');
  await summary.click();await page.locator('#connect-open').click();await page.locator('#connect-dialog').waitFor({state:'visible'});await page.locator('#connect-close').click();
  assert.equal(requests.filter(([method])=>method!=='GET').length,beforeMenu,'opening and navigating the menu never sends commands');
  assert.equal(await page.locator('#main-nav [data-view="espwifi"]').count(),0);
  await summary.click();await page.locator('#panel-tools [data-view="espwifi"]').click();
  assert.equal(await page.locator('[data-section="espwifi"]').isVisible(),true);
  assert.equal(await tools.getAttribute('open'),null);
  await page.locator('#wifi-settings-open').click();await page.locator('#wifi-dialog').waitFor({state:'visible'});
  assert.equal(requests.filter(([method])=>method!=='GET').length,beforeMenu,'opening Wi-Fi never scans or saves on its own');
  const wifiWrites=[];let scanSequence=1;
  await page.route('**/api/wifi/scan',async route=>{
    if(route.request().method()==='POST'){wifiWrites.push({path:'scan'});scanSequence++;await route.fulfill({json:{message:'Scanning (DEMO)'}});}
    else await route.fulfill({json:{sequence:scanSequence,busy:false,result:'ESP_OK',networks:[{ssid:'Moja_siec_24GHz',rssi:-49,authmode:3},{ssid:'Siec_otwarta',rssi:-70,authmode:0}]}});
  });
  await page.route('**/api/wifi',async route=>{wifiWrites.push(route.request().postDataJSON());await route.fulfill({json:{message:'Saved (DEMO)'}});});
  await page.locator('#wifi-search').click();await page.locator('#wifi-select option').getByText('Moja_siec_24GHz',{exact:false}).waitFor({state:'attached'});
  await page.locator('#wifi-select').selectOption('Moja_siec_24GHz');await page.locator('#wifi-password').fill('example-wifi-password');
  await page.evaluate(()=>EcoWifi.render({connected:true,ssid:'Old-network',ip:'192.0.2.10',hostname:'piec'}));
  assert.equal(await page.locator('#wifi-select').inputValue(),'Moja_siec_24GHz','poll preserves selected SSID');
  assert.equal(await page.locator('#wifi-password').inputValue(),'example-wifi-password','poll preserves password draft');
  await screenshot('11-zmiana-wifi');
  await page.locator('#wifi-save').click();await page.waitForFunction(()=>document.getElementById('wifi-result').textContent.startsWith('Zapisano'));
  assert.deepEqual(wifiWrites,[{path:'scan'},{ssid:'Moja_siec_24GHz',password:'example-wifi-password'}]);
  assert.equal(await page.locator('#wifi-password').inputValue(),'');
  await page.locator('#wifi-select').selectOption('Siec_otwarta');assert.equal(await page.locator('#wifi-password').isDisabled(),true);
  await page.locator('#wifi-select').selectOption({index:3});
  assert.equal(await page.locator('#wifi-manual-field').isVisible(),true);await page.locator('#wifi-ssid').fill('Hidden-network');
  await page.locator('#wifi-password').fill('example-wifi-password');
  await page.route('**/api/wifi',async route=>{wifiWrites.push(route.request().postDataJSON());await route.abort();});
  await page.locator('#wifi-save').click();await page.waitForFunction(()=>document.getElementById('wifi-result').textContent.includes('Nie ponowiono'));
  assert.equal(wifiWrites.length,3,'unknown outcome never causes a retry');assert.equal(await page.locator('#wifi-password').inputValue(),'');
  await page.locator('#wifi-password').fill('do-not-keep');await page.locator('#wifi-close').click();
  assert.equal(await page.locator('#wifi-password').inputValue(),'','closing clears password');
  await summary.click();await page.locator('#analysis-export').click();
  assert.equal(await page.locator('#analysis-hours').inputValue(),'72');
  await screenshot('13-analiza-json');await page.locator('#analysis-close').click();
  await page.locator('[data-view="live"]').click();
  await page.locator('[data-view="schema"]').click();await page.locator('#schema-graphic svg').waitFor();
  assert.equal(await page.locator('#lbSchema_tempCO').textContent(),'72,5 \u00b0C');
  assert.equal(await page.locator('#schema_pumpMixerA').getAttribute('data-state'),'on');
  assert.equal(await page.locator('#schema_pumpCWU').getAttribute('data-state'),'off');
  assert.deepEqual(await page.locator('#schema-graphic svg text').evaluateAll(nodes=>nodes.filter(node=>{
    const b=node.getBBox(),matrix=node.ownerSVGElement.getScreenCTM().inverse().multiply(node.getScreenCTM());
    return [[b.x,b.y],[b.x+b.width,b.y+b.height]].some(([x,y])=>{const p=new DOMPoint(x,y).matrixTransform(matrix);return p.x<0||p.y<0||p.x>841.66||p.y>437.104;});
  }).map(node=>node.textContent)),[],'schematic text fits drawing');
  await screenshot('10-schemat');
  await page.evaluate(()=>EcoHeatingSchema.render({...state,curr:{...state.curr,fuelLevel:null,fuelCalibration:true}},true));
  assert.equal(await page.locator('#lbSchema_fuelLevel').textContent(),'Kalibracja');
  await page.evaluate(()=>EcoHeatingSchema.render({...state,curr:{...state.curr,fuelCalibration:true}},false));
  assert.equal(await page.locator('#lbSchema_fuelLevel').textContent(),'---');
  await page.evaluate(()=>EcoHeatingSchema.render(state,true));
  await page.locator('#update-open').click();
  await page.locator('#update-current').getByText(version,{exact:true}).waitFor();
  await page.locator('.update-panel').scrollIntoViewIfNeeded();
  await screenshot('12-aktualizacje');
  await page.locator('.esp-resources').evaluate(el=>el.scrollIntoView({block:'start'}));
  assert.ok((await page.locator('.esp-resources h3').boundingBox()).y>(await page.locator('header').boundingBox()).height,'memory heading is below the fixed header');
  await screenshot('14-pamiec-esp');
  const advancedDemo=[[158,'Blokada kotla od termostatu','Tak'],[161,'Praca na czas','Tak'],[44,'Funkcja histerezy','Wylaczona LATO'],[151,'Detekcja obrotow wentylatora',0],[46,'Tryb czujnika podajnika','Podajnik i ruszt'],[45,'Kondensacja','OFF'],[162,'Tryb KOMINIARZ','OFF'],[7,'Sterowanie pogodowe kotla','Wylaczone'],[166,'Pompa ciepla','OFF']];
  state.menu=advancedDemo.map(([index,name,value])=>({index,name,advanced:true,in_structure:true,locked:false,metadata:{value},write_id:'rm:'+index,write_enabled:true}));
  for(const item of state.menu)state.targets[item.write_id]={name:item.name,before:0,min:0,max:1,step:1,value_kind:'enum',options:[{value:0,label:String(item.metadata.value)}],risk:true,advanced:true};
  await page.evaluate(()=>poll());await page.locator('#main-nav [data-view="service"]').click();
  await page.locator('[data-section="service"] [data-view="advanced"]').click();
  assert.equal(await page.locator('#advanced-tree .parameter-row').count(),9);
  await screenshot('15-zaawansowane');
  state.menu=[];
  await page.locator('[data-view="schema"]').click();
  await page.evaluate(()=>EcoHeatingSchema.render(state,false));
  assert.equal(await page.locator('#lbSchema_tempCO').textContent(),'---');
  assert.equal(await page.locator('#schema_pumpMixerA').getAttribute('data-state'),'unknown');
  await page.evaluate(()=>EcoHeatingSchema.render({...state,system:{schemaID:99}},true));
  assert.equal(await page.locator('#schema-graphic').isVisible(),false);
  assert.ok((await page.locator('#schema-readings').textContent()).includes('Brak schematu'));
  await page.evaluate(()=>EcoHeatingSchema.render(state,true));
  for(const width of [320,390,1280]){
    await page.setViewportSize({width,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'diagram scroll is local, not full-page');
    await page.evaluate(()=>EcoWifi.render(state.esp_network));
    await summary.click();await page.locator('#wifi-open').click();
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'Wi-Fi dialog fits phone');
    await page.locator('#wifi-close').click();
  }
  await page.setViewportSize({width:1280,height:1080});
  await page.locator('[data-view="live"]').click();
  assert.ok((await page.locator('main').evaluate(el=>getComputedStyle(el).backgroundImage)).includes('devicebg.jpg'));
  assert.equal(await page.evaluate(async()=>{const i=new Image();i.src='/assets/devicebg.jpg';await i.decode();return i.naturalWidth>1000;}),true,'restored background bitmap decodes');
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
  await page.evaluate(()=>EcoUpdates.load());
  await page.locator('#update-open-label').getByText('Aktualne',{exact:true}).waitFor();
  assert.deepEqual(await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member)),customOrder,'order survives reload and fresh data');
  await page.locator('#tile-layout-edit').click();await resetLayout();
  assert.deepEqual(await page.locator('.eco-tile').evaluateAll(els=>els.map(el=>el.dataset.member)),originalOrder);
  await page.locator('#tile-layout-edit').click();
  await page.evaluate(() => notice('DEMO - dane przykladowe, nie odczyt prawdziwego kotla.'));
  await page.locator('[data-view="pellet"]').click();
  assert.equal(await page.locator('#pellet-level').textContent(),'68 %');
  assert.equal(await page.locator('#pellet-hopper .pellet-fill').count(),1);
  await screenshot('07-pellet');
  state.pellet.level_percent=null;state.pellet.level_calibrating=true;state.curr.fuelLevel=null;state.curr.fuelCalibration=true;
  await page.evaluate(()=>poll());assert.equal(await page.locator('#pellet-level').textContent(),'Kalibracja');
  await screenshot('16-kalibracja-paliwa');
  state.pellet.level_percent=68;state.pellet.level_calibrating=false;state.curr.fuelLevel=68;state.curr.fuelCalibration=false;
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
  await page.route('**/api/updates',route=>route.fulfill({json:{current:'0.7.0-beta.7',version,channel:'all',beta:true,available:true,install_supported:true,automatic:true,phase:'available',busy:false}}));
  await page.evaluate(()=>EcoUpdates.load());
  assert.equal(await page.locator('#update-open-label').innerText(),'Aktualizacja');
  for(const width of [320,390,1280]){
    await page.setViewportSize({width,height:844});
    assert.equal(await page.locator('#update-open').isVisible(),true);assert.equal(await page.locator('#update-notice-beta').innerText(),'BETA');
    const bar=await page.locator('.page-toolbar').boundingBox(),button=await page.locator('#update-open').boundingBox();
    assert.ok(button.x>=bar.x&&button.x+button.width<=bar.x+bar.width+1&&button.y>=bar.y&&button.y+button.height<=bar.y+bar.height,'available update remains within the shared toolbar');
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'toolbar including BETA update fits');
    await summary.click();const menuBox=await tools.locator('.tools-list').boundingBox();assert.ok(menuBox.x>=0&&menuBox.x+menuBox.width<=width,'tools still fits beside update');await page.keyboard.press('Escape');
  }
  await page.locator('#update-open').click();
  assert.equal(await page.locator('[data-section="settings"]').isVisible(),true,'toolbar update opens settings only');
  let accessWrites=0;
  await page.route('**/api/access',route=>{accessWrites++;return route.fulfill({json:{message:'Saved (DEMO)'}});});
  for(const width of [320,390,1280]){
    await page.setViewportSize({width,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'password fields fit viewport');
    assert.equal(await page.locator('#access-current').getAttribute('type'),'password');
  }
  await page.locator('#access-current').fill('admin');await page.locator('#access-password').fill('example-password');await page.locator('#access-repeat').fill('example-password');
  page.once('dialog',dialog=>dialog.accept());await page.locator('#access-save').click();
  await page.waitForFunction(()=>document.getElementById('access-status').textContent==='Saved (DEMO)');
  assert.equal(accessWrites,1);assert.equal(await page.locator('#access-password').inputValue(),'');
  await page.unroute('**/api/access');
  assert.equal(requests.filter(([,path])=>path==='/api/updates/install').length,0,'toolbar update never installs by itself');
  await page.locator('[data-view="live"]').click();
  await page.unroute('**/api/updates');
  await page.route('**/api/updates',route=>route.fulfill({json:{current:'0.6.40-update-toolbar',current_release:version,version,channel:'all',beta:true,available:false,install_supported:false,phase:'up_to_date',checked:now}}));
  await page.evaluate(()=>EcoUpdates.load());assert.equal(await page.locator('#update-open-label').innerText(),'Aktualne');
  assert.equal(await page.locator('#update-install').isVisible(),false);
  await page.unroute('**/api/updates');await page.route('**/api/updates',route=>route.abort());
  await page.evaluate(()=>EcoUpdates.load());assert.equal(await page.locator('#update-open-label').innerText(),'Aktualizacje','failed request never claims up to date');
  await page.unroute('**/api/updates');await page.evaluate(()=>EcoUpdates.load());await page.setViewportSize({width:390,height:844});
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
  console.log(`PASS sixteen panel screenshots for ${version}: fuel calibration, advanced menu, memory/device parameters, JSON export dialog, five Wi-Fi bars, collapsed diagnostics, direct Wi-Fi scan/save/unknown outcome, schema fresh/stale/pumps, background decoded, named tools, logout last, ordering persisted/reset, pellet states, charts nonblank, 320/390/1280px fit.`);
} finally {
  await browser.close();
  await new Promise(done => server.close(done));
}
