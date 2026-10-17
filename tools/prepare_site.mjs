import { readFile, writeFile, copyFile, mkdir, cp } from 'node:fs/promises';
import { createHash } from 'node:crypto';

const root = new URL('../', import.meta.url);
const release = JSON.parse(await readFile(new URL('dist/manifest.json', root)));
const binary = await readFile(new URL('dist/firmware-merged.bin', root));
if (release.chip !== 'esp32s3' || release.flash_mb !== 16 || release.psram_mb !== 8 || !release.nvs_blank)
  throw new Error('Incompatible first-install image');
if (createHash('sha256').update(binary).digest('hex') !== release.files['firmware-merged.bin'].sha256)
  throw new Error('Firmware hash mismatch');
const manifest = {
  name: 'ecoMAX Wi-Fi', version: release.version,
  new_install_prompt_erase: true, new_install_improv_wait_time: 0,
  builds: [{ chipFamily: 'ESP32-S3', parts: [{ path: 'firmware-merged.bin', offset: 0 }] }],
};
await mkdir(new URL('site/', root), { recursive: true });
await copyFile(new URL('dist/firmware-merged.bin', root), new URL('site/firmware-merged.bin', root));
await writeFile(new URL('site/manifest.json', root), JSON.stringify(manifest, null, 2) + '\n');
await writeFile(new URL('site/.nojekyll', root), '');
await copyFile(new URL('docs/START_PL.html', root), new URL('site/instrukcja.html', root));
await copyFile(new URL('docs/SZYBKI_START.html', root), new URL('site/szybki-start.html', root));
for (const stem of ['START_PL', 'SZYBKI_START', 'GALERIA', 'SPRZET', 'ZGODNOSC'])
  await copyFile(new URL('docs/' + stem + '.html', root), new URL('site/' + stem + '.html', root));
await cp(new URL('docs/images/', root), new URL('site/images/', root), { recursive: true });
console.log(`Installer ready: ${release.version}; only ESP32-S3 16 MB / 8 MB PSRAM`);
