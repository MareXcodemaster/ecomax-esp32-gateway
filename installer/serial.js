export function validateWifi(ssid, password) {
  const length = value => new TextEncoder().encode(value).length;
  if (!ssid || length(ssid) > 32 || /[\x00-\x1f\x7f]/.test(ssid))
    throw new Error('Nazwa sieci musi miec od 1 do 32 bajtow, bez znakow sterujacych.');
  if (password && (length(password) < 8 || length(password) > 63 || /[\x00-\x1f\x7f]/.test(password)))
    throw new Error('Haslo Wi-Fi: 8-63 bajty, albo puste dla sieci otwartej.');
}

export function panelUrl(ip) {
  if (typeof ip !== 'string' || !/^(\d{1,3}\.){3}\d{1,3}$/.test(ip)) return null;
  const bytes = ip.split('.').map(Number);
  if (bytes.some(x => x > 255) || bytes[0] === 0 || bytes[0] === 127 || bytes[0] >= 224 || bytes[3] === 255) return null;
  return `http://${bytes.join('.')}/`;
}

export class DeviceSerial {
  constructor() { this.port = null; this.pending = null; this.buffer = ''; this.failure = null; }
  async open(port) {
    if (this.port) throw new Error('Port jest juz otwarty.');
    await port.open({ baudRate: 115200, bufferSize: 16384 });
    this.port = port;
    await port.setSignals?.({ dataTerminalReady: true, requestToSend: false });
    this.reader = port.readable.getReader();
    this.writer = port.writable.getWriter();
    this.reading = this.readLoop();
  }
  async readLoop() {
    const decoder = new TextDecoder();
    try {
      for (;;) {
        const { value, done } = await this.reader.read();
        if (done) break;
        this.buffer += decoder.decode(value, { stream: true });
        let end;
        while ((end = this.buffer.indexOf('\n')) >= 0) {
          const line = this.buffer.slice(0, end);
          this.buffer = this.buffer.slice(end + 1);
          const marker = line.indexOf('[USB] ');
          if (marker < 0) continue;
          let reply;
          try { reply = JSON.parse(line.slice(marker + 6)); } catch { continue; }
          const pending = this.pending;
          if (pending && pending.matches(reply)) pending.resolve(reply);
        }
        if (this.buffer.length > 65536) this.buffer = '';
      }
      throw new Error('Polaczenie USB zostalo zamkniete.');
    } catch (error) {
      this.failure = new Error('Utracono polaczenie USB. Podlacz urzadzenie ponownie.');
      this.pending?.reject(this.failure);
    }
  }
  async request(command, payload = {}, timeout = 5000) {
    if (!this.port || this.failure) throw this.failure || new Error('Najpierw polacz ESP przez USB.');
    if (this.pending) throw new Error('Trwa poprzednie polecenie USB.');
    let timer;
    const response = new Promise((resolve, reject) => {
      timer = setTimeout(() => reject(new Error('ESP nie odpowiedzial. Zamknij okno wgrywania i nacisnij RESET na plytce.')), timeout);
      this.pending = { resolve, reject, matches: r => command === 'status' ? !!r.wifi : r.command === command };
    });
    // Attach the rejection handler before asynchronous writing (a disconnect can happen during write).
    const writing = this.writer.write(new TextEncoder().encode(JSON.stringify({ ...payload, command }) + '\n'));
    try { return (await Promise.all([response, writing]))[0]; }
    finally { clearTimeout(timer); this.pending = null; }
  }
  async setup() {
    const reply = await this.request('setup', {}, 12000);
    if (reply.username !== 'admin' || !/^[a-f0-9]{32}$/.test(reply.password || ''))
      throw new Error('To nie jest zgodny publiczny firmware. Nie wyslano hasla Wi-Fi.');
    return reply;
  }
  async configure(ssid, password) {
    validateWifi(ssid, password);
    // A write is sent once. Only read-only status is polled afterwards.
    const reply = await this.request('wifi', { ssid, password }, 12000);
    if (reply.ok !== true) throw new Error('ESP odrzucilo ustawienia Wi-Fi. Sprawdz nazwe i haslo.');
  }
  async close() {
    this.pending?.reject(new Error('Rozlaczono USB.'));
    if (this.reader) {
      await this.reader.cancel().catch(() => {});
      await this.reading;
      this.reader.releaseLock();
    }
    this.writer?.releaseLock();
    await this.port?.close().catch(() => {});
    this.port = null; this.reader = null; this.writer = null; this.buffer = '';
  }
}
