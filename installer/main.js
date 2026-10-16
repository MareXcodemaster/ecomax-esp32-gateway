import 'esp-web-tools/dist/web/install-button.js';
import { createIcons, Cpu, Unplug, Download, Usb, Wifi, Eye, KeyRound, Copy, ExternalLink } from 'lucide';
import './style.css';
import { DeviceSerial, validateWifi, panelUrl } from './serial.js';

const $ = id => document.getElementById(id);
createIcons({ icons: { Cpu, Unplug, Download, Usb, Wifi, Eye, KeyRound, Copy, ExternalLink } });
let device = null, busy = false, pollGeneration = 0;
const supported = !!navigator.serial && window.isSecureContext;
$('unsupported').hidden = supported;
$('connect').disabled = !supported;
function status(message, error = false) {
  $('status').textContent = message;
  $('status').dataset.error = String(error);
}
function clearAddress() {
  $('open-panel').hidden = true;
  $('open-panel').removeAttribute('href');
  $('device-address').textContent = 'Adres panelu pojawi sie po polaczeniu ESP z Wi-Fi.';
}
function showAddress(wifi) {
  const url = wifi?.connected ? panelUrl(wifi.ip) : null;
  if (!url) { clearAddress(); return false; }
  $('open-panel').href = url;
  $('open-panel').hidden = false;
  const hostname = typeof wifi.hostname === 'string' && wifi.hostname.length <= 63 && /^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/.test(wifi.hostname) ? wifi.hostname : 'piec';
  $('device-address').textContent = `Adres: ${url} (komputer lub telefon musi byc w tej samej sieci). Adres lokalny: http://${hostname}.local/`;
  return true;
}
function updateButtons() {
  $('install').disabled = !supported || !$('compatible').checked || !!device || busy;
  $('connect').disabled = !supported || !!device || busy;
  $('disconnect').disabled = busy;
  $('save-wifi').disabled = busy;
}
$('compatible').addEventListener('change', updateButtons);
function forget() {
  pollGeneration++;
  $('device-password').value = '';
  $('device-password').type = 'password';
  $('wifi-password').value = '';
  $('credentials').hidden = true;
  $('wifi-form').hidden = true;
  $('disconnect').hidden = true;
  clearAddress();
}
async function disconnect() {
  const previous = device;
  device = null;
  forget();
  await previous?.close();
  updateButtons();
}
$('disconnect').addEventListener('click', async () => {
  await disconnect(); status('Rozlaczono USB. Ustawienia pozostaja w ESP.');
});
$('connect').addEventListener('click', async () => {
  if (busy || device) return;
  busy = true; updateButtons(); clearAddress();
  const candidate = new DeviceSerial();
  try {
    const port = await navigator.serial.requestPort();
    status('Odczytywanie danych ESP...');
    await candidate.open(port);
    device = candidate;
    const setup = await candidate.setup();
    $('device-password').value = setup.password;
    $('credentials').hidden = false;
    $('wifi-form').hidden = false;
    $('disconnect').hidden = false;
    status('ESP polaczone. Wpisz swoja siec Wi-Fi 2,4 GHz.');
    const reply = await candidate.request('status');
    if (typeof reply.wifi.ssid === 'string') $('ssid').value = reply.wifi.ssid;
    if (showAddress(reply.wifi)) status('ESP jest juz polaczone z Wi-Fi. Mozesz otworzyc panel lub zmienic siec.');
  } catch (error) {
    await candidate.close(); device = null; forget();
    status(error.name === 'NotFoundError' ? 'Nie wybrano portu USB.' : error.message, true);
  } finally { busy = false; updateButtons(); }
});
$('wifi-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (busy || !device) return;
  const ssid = $('ssid').value, password = $('wifi-password').value;
  try { validateWifi(ssid, password); } catch (error) { status(error.message, true); return; }
  busy = true; updateButtons(); clearAddress();
  const current = device, generation = ++pollGeneration;
  try {
    status('Zapisywanie sieci w ESP...');
    await current.configure(ssid, password);
    $('wifi-password').value = '';
    status('Siec zapisana. Czekam na polaczenie Wi-Fi...');
    for (let attempt = 0; attempt < 25; attempt++) {
      await new Promise(resolve => setTimeout(resolve, 1500));
      if (generation !== pollGeneration) return;
      const reply = await current.request('status');
      if (reply.wifi.ssid === ssid && showAddress(reply.wifi)) {
        status('Gotowe. ESP polaczone z Twoim Wi-Fi. Otworz panel.');
        return;
      }
    }
    status('Siec zostala zapisana, ale ESP nie uzyskalo adresu. Sprawdz haslo, zasieg i siec 2,4 GHz. Mozesz poprawic dane i zapisac ponownie.', true);
  } catch (error) { status(error.message, true); }
  finally { $('wifi-password').value = ''; busy = false; updateButtons(); }
});
for (const [buttonId, inputId, label] of [['show-wifi', 'wifi-password', 'Wi-Fi'], ['show-device', 'device-password', 'ESP']]) {
  $(buttonId).addEventListener('click', () => {
    const visible = $(inputId).type === 'password';
    $(inputId).type = visible ? 'text' : 'password';
    $(buttonId).title = `${visible ? 'Ukryj' : 'Pokaz'} haslo ${label}`;
    $(buttonId).setAttribute('aria-label', $(buttonId).title);
    $(buttonId).setAttribute('aria-pressed', String(visible));
  });
}
$('copy-password').addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText($('device-password').value);
    status('Haslo ESP skopiowane. Wklej je do swojego menedzera hasel.');
  } catch { status('Nie udalo sie skopiowac. Odkryj haslo przyciskiem oka i skopiuj recznie.', true); }
});
navigator.serial?.addEventListener('disconnect', async event => {
  if (event.target === device?.port || event.port === device?.port) {
    await disconnect(); status('Odlaczono ESP od USB. Podlacz je ponownie, aby kontynuowac.', true);
  }
});
window.addEventListener('pagehide', () => { forget(); void device?.close(); });
updateButtons();
