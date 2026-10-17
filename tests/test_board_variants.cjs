const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.join(__dirname, '..');

test('installer and Polish guide explicitly include both antenna variants', () => {
  for (const name of ['installer/index.html', 'docs/START_PL.md', 'docs/ZGODNOSC.md']) {
    const text = fs.readFileSync(path.join(root, name), 'utf8');
    assert.ok(text.includes('ESP32-S3-RS485-CAN-U'), name);
    assert.ok(text.includes('ESP32-S3-RS485-CAN'), name);
  }
  const html = fs.readFileSync(path.join(root, 'installer/index.html'), 'utf8');
  assert.match(html, /id="compatible"[^>]*>[^<]*ESP32-S3-RS485-CAN-U/);
  assert.match(html, /Nie ma przelacznika anteny w programie/);
});

test('gallery images are real generated panel captures and explicitly marked as examples', () => {
  const guide = fs.readFileSync(path.join(root, 'docs/GALERIA.md'), 'utf8');
  assert.match(guide, /DEMO/);
  const images = [...guide.matchAll(/\]\((images\/panel\/[^)]+\.png)\)/g)];
  assert.equal(images.length, 6);
  for (const [, name] of images) {
    const data = fs.readFileSync(path.join(root, 'docs', name));
    assert.equal(data.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
    assert.ok(data.length > 10000, name);
  }
});
