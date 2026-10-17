import test from 'node:test';
import assert from 'node:assert/strict';
import { DeviceSerial, validateWifi, panelUrl } from '../installer/serial.js';

function port(handler) {
  const state = { writes: [] };
  const encoder = new TextEncoder();
  const readable = new ReadableStream({ start(controller) { state.controller = controller; } });
  return {
    readable, state,
    writable: new WritableStream({ write(value) {
      const command = JSON.parse(new TextDecoder().decode(value));
      state.writes.push(command);
      const response = handler(command);
      if (response) state.controller.enqueue(encoder.encode('boot log\n[USB] ' + JSON.stringify(response) + '\n'));
    }}),
    async open(options) { state.options = options; }, async close() { state.closed = true; },
    async setSignals(signals) { state.signals = signals; },
  };
}
test('Wi-Fi validation counts UTF-8 bytes and never trims a valid SSID', () => {
  validateWifi(' Example ', 'abcdefgh'); validateWifi('Open', '');
  for (const [ssid, password] of [['', ''], ['x'.repeat(33), ''], ['x', 'short'], ['x', 'y'.repeat(64)], ['x\n', 'abcdefgh'], ['ą'.repeat(17), 'abcdefgh']])
    assert.throws(() => validateWifi(ssid, password));
});
test('panel URL cannot inject a different scheme, credentials or markup', () => {
  assert.equal(panelUrl('192.0.2.10'), 'http://192.0.2.10/');
  for (const value of ['javascript:alert(1)', '192.0.2.1@example.org', '999.0.0.1', '<script>', '0.0.0.0', '127.0.0.1', null])
    assert.equal(panelUrl(value), null);
});
test('USB verifies firmware, writes Wi-Fi once, accepts only matching replies', async () => {
  const serial = new DeviceSerial();
  const usb = port(command => command.command === 'setup' ? { command: 'setup', username: 'admin', password: '0123456789abcdef'.repeat(2) }
    : command.command === 'wifi' ? { command: 'wifi', ok: true } : { wifi: { connected: true, ip: '192.0.2.10' } });
  await serial.open(usb);
  assert.equal((await serial.setup()).username, 'admin');
  await serial.configure('Example', 'abcdefgh');
  assert.equal((await serial.request('status')).wifi.connected, true);
  assert.deepEqual(usb.state.writes.map(v => v.command), ['setup', 'wifi', 'status']);
  assert.equal(usb.state.signals.dataTerminalReady, true);
  await serial.close(); assert.equal(usb.state.closed, true);
});
test('new firmware uses simple defaults and separate AP password',async()=>{
 const usb=port(()=>({command:'setup',username:'admin',password:'admin',ap_password:'piecwifi',access_schema:1}));
 const serial=new DeviceSerial();await serial.open(usb);const setup=await serial.setup();
 assert.equal(setup.password,'admin');assert.equal(setup.ap_password,'piecwifi');await serial.close();
});
test('incompatible firmware and invalid Wi-Fi do not send a write', async () => {
  const serial = new DeviceSerial();
  const usb = port(() => ({ command: 'setup', username: 'admin', password: 'invalid' }));
  await serial.open(usb);
  await assert.rejects(serial.setup(), /zgodny/);
  await assert.rejects(serial.configure('Example', 'short'));
  assert.equal(usb.state.writes.length, 1);
  await serial.close();
});
test('USB timeout does not retry a command', async () => {
  const serial = new DeviceSerial(); const usb = port(() => null);
  await serial.open(usb);
  await assert.rejects(serial.request('setup', {}, 10), /nie odpowiedzial/);
  assert.equal(usb.state.writes.length, 1);
  await serial.close();
});
