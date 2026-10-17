import { readFile, writeFile } from 'node:fs/promises';
import { Marked } from 'marked';

const root = new URL('../', import.meta.url);
const pages = { START_PL: 'Pelna instrukcja', SZYBKI_START: 'Prosta instalacja',
  GALERIA: 'Galeria panelu', SPRZET: 'Sprzet i podlaczenie', ZGODNOSC: 'Lista zgodnosci' };
const markdown = new Marked({ gfm: true });
markdown.use({ renderer: { link(token) {
  let href = token.href;
  if (!href.startsWith('#') && !/^[a-z]+:/i.test(href)) {
    const [name, anchor] = href.split('#'), stem = name.replace(/\.md$/, '');
    href = Object.hasOwn(pages, stem) ? stem + '.html' + (anchor ? '#' + anchor : '')
      : new URL(href, 'https://github.com/MareXcodemaster/ecomax-esp32-gateway/blob/main/docs/').href;
  }
  const escaped = href.replaceAll('&', '&amp;').replaceAll('"', '&quot;');
  return `<a href="${escaped}">${this.parser.parseInline(token.tokens)}</a>`;
}} });
for (const [stem, title] of Object.entries(pages)) {
const source = await readFile(new URL('docs/' + stem + '.md', root), 'utf8');
const html = `<!doctype html>
<html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="referrer" content="no-referrer">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; img-src 'self' data: https://www.waveshare.com https://docs.waveshare.net; base-uri 'none'; form-action 'none'">
<title>ecoMAX Wi-Fi - ${title}</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#fff;color:#243138;font:16px/1.65 system-ui,"Segoe UI",Arial,sans-serif;letter-spacing:0}header{border-bottom:1px solid #d7e1e3;padding:16px 24px}header nav{max-width:960px;margin:auto;display:flex;flex-wrap:wrap;gap:20px}main{max-width:960px;margin:auto;padding:24px}h1{font-size:30px;line-height:1.25}h2{font-size:23px;line-height:1.35;margin-top:40px;padding-top:18px;border-top:1px solid #dce6e7}p,li{overflow-wrap:anywhere}a{color:#176a85}img{display:block;max-width:100%;height:auto;margin:22px auto;border:1px solid #d8e1e3;border-radius:4px}table{width:100%;border-collapse:collapse;margin:20px 0;font-size:14px}th,td{border:1px solid #ccd9dc;padding:10px;text-align:left;vertical-align:top;overflow-wrap:anywhere}th{background:#eef6f4}pre{white-space:pre-wrap;overflow-wrap:anywhere}em{font-size:14px;color:#546970}strong{font-weight:700}@media(max-width:600px){main{padding:18px}header{padding:16px 18px}h1{font-size:26px}h2{font-size:21px}th,td{padding:7px;font-size:13px}}@media print{header{display:none}main{max-width:none;padding:0}h2{break-after:avoid}img{max-height:18cm;object-fit:contain}tr{break-inside:avoid}a{color:inherit}body{font-size:11pt}}
</style></head><body><header><nav><a href="https://marexcodemaster.github.io/ecomax-esp32-gateway/">Instalator USB</a><a href="https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases">Pliki do pobrania</a></nav></header><main>${markdown.parse(source)}</main></body></html>\n`;
await writeFile(new URL('docs/' + stem + '.html', root), html);
}
console.log('Five Polish HTML guides generated: simple start, full guide, gallery, hardware and compatibility.');
