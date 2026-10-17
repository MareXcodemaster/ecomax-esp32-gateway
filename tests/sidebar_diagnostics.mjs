// Native disclosure behaviour with the real sidebar and CSS, no device requests.
import assert from 'node:assert/strict';
import {readFile,mkdir} from 'node:fs/promises';
import {chromium} from 'playwright';
const source=await readFile('src/panel_web/index.html','utf8');
const nav=source.match(/<nav id="main-nav"[\s\S]*?<\/nav>/)[0];
const styles=(await Promise.all(['style','mirror','compact'].map(name=>readFile(`src/panel_web/${name}.css`,'utf8')))).join('\n');
const browser=await chromium.launch({headless:true});await mkdir('test-results',{recursive:true});
try{
 for(const width of [320,390,1280]){
  const page=await browser.newPage({viewport:{width,height:900},hasTouch:true});
  await page.setContent('<!doctype html><html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><body class="nav-open"><header>ecoMAX DEMO</header><div class="layout">'+nav+'<main>Panel DEMO</main></div></body></html>');
  await page.addStyleTag({content:styles});await page.addScriptTag({content:await readFile('src/panel_web/lucide.js','utf8')});
  await page.evaluate(()=>lucide.createIcons());
  const group=page.locator('#local-diagnostics'),toggle=group.locator('summary');
  assert.equal(await group.getAttribute('open'),null,'collapsed by default');
  for(const view of ['data','menu','api','tests'])assert.equal(await group.locator(`[data-view="${view}"]`).isVisible(),false);
  assert.equal(await page.locator('#address').isVisible(),false);
  assert.equal(await page.locator('[data-view="settings"]').isVisible(),true);
  await toggle.click();assert.notEqual(await group.getAttribute('open'),null);
  for(const view of ['data','menu','api','tests'])assert.equal(await group.locator(`[data-view="${view}"]`).isVisible(),true);
  assert.equal(await page.locator('#address').isVisible(),true);
  const bounds=await toggle.boundingBox();assert.ok(bounds.width<=238&&bounds.height>=44);
  assert.equal(await toggle.evaluate(el=>el.scrollWidth>el.clientWidth+1),false,'heading fits');
  assert.equal(await group.locator('svg.lucide-chevron-right').count(),1);
  await page.screenshot({path:`test-results/sidebar-diagnostics-open-${width}.png`});
  await toggle.focus();await page.keyboard.press('Enter');assert.equal(await group.getAttribute('open'),null);
  await page.keyboard.press('Space');assert.notEqual(await group.getAttribute('open'),null);
  await toggle.tap();assert.equal(await group.getAttribute('open'),null);
  await page.screenshot({path:`test-results/sidebar-diagnostics-closed-${width}.png`});
  await page.close();console.log(`PASS sidebar ${width}px: initially collapsed, all lower items, mouse, keyboard and touch`);
 }
}finally{await browser.close();}
