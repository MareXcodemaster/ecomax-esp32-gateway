import { readdir, readFile, copyFile, mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { join } from 'node:path';

const packages = 'node_modules/.pnpm';
const manifest = {};
for (const folder of await readdir(packages)) {
  const modules = join(packages, folder, 'node_modules');
  let entries;
  try { entries = await readdir(modules, { withFileTypes: true }); } catch { continue; }
  const roots = [];
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    if (entry.name.startsWith('@')) {
      for (const scoped of await readdir(join(modules, entry.name), { withFileTypes: true }))
        if (scoped.isDirectory()) roots.push(join(modules, entry.name, scoped.name));
    } else roots.push(join(modules, entry.name));
  }
  for (const directory of roots) {
    let info;
    try { info = JSON.parse(await readFile(join(directory, 'package.json'))); } catch { continue; }
    const target = join('LICENSES/installer', info.name.replaceAll('/', '__') + '-' + info.version);
    for (const name of await readdir(directory)) {
      if (!/^(license|licence|notice|copying)([._-].*)?$/i.test(name)) continue;
      const bytes = await readFile(join(directory, name));
      await mkdir(target, { recursive: true });
      await copyFile(join(directory, name), join(target, name));
      manifest[join(target, name).replaceAll('\\', '/')] = createHash('sha256').update(bytes).digest('hex');
    }
  }
}
await writeFile('LICENSES/installer-manifest.json', JSON.stringify(manifest, null, 2) + '\n');
console.log(`Collected ${Object.keys(manifest).length} installer / build-tool license notices`);
