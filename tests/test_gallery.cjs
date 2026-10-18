const test = require('node:test');
const assert = require('node:assert/strict');
const {readFileSync, readdirSync} = require('node:fs');
const {resolve, dirname} = require('node:path');

const root = resolve(__dirname, '..');
const {version} = JSON.parse(readFileSync(resolve(root, 'package.json'), 'utf8'));

async function panelImages(file) {
  const {Marked} = await import('marked');
  const parser = new Marked(), images = [];
  parser.walkTokens(parser.lexer(readFileSync(resolve(root, file), 'utf8')), token => {
    if (token.type === 'image' && token.href.includes('images/panel/')) images.push(token.href);
  });
  return images;
}

test('gallery describes the current firmware and includes every current screenshot', async () => {
  const gallery = readFileSync(resolve(root, 'docs/GALERIA.md'), 'utf8');
  assert.ok(gallery.includes(`**${version}**`));
  const images = await panelImages('docs/GALERIA.md');
  const files = readdirSync(resolve(root, 'docs/images/panel', version)).filter(file => file.endsWith('.png'));
  assert.ok(files.length >= 12);
  assert.deepEqual(images.map(href => href.split('/').at(-1)).sort(), files.sort());
});

for (const file of ['README.md', 'docs/GALERIA.md', 'docs/START_PL.md']) {
  test(`${file}: panel previews are versioned, present PNG images`, async () => {
    const images = await panelImages(file);
    assert.ok(images.length > 0);
    for (const href of images) {
      assert.ok(href.includes(`/panel/${version}/`), href);
      const png = readFileSync(resolve(root, dirname(file), href));
      assert.equal(png.subarray(0, 8).toString('hex'), '89504e470d0a1a0a', href);
      assert.ok(png.readUInt32BE(16) >= 320 && png.readUInt32BE(20) >= 600, href);
    }
  });
}
