/*
 * serial_bridge.js
 * Reads JSON lines from ESP32 over Serial → rebroadcasts over WebSocket.
 *
 * Usage:
 *   npm install serialport ws
 *   node serial_bridge.js
 *   Then open index.html in a browser.
 *
 * Find your port:
 *   Mac/Linux:  ls /dev/tty.usb*   or   ls /dev/ttyUSB*
 *   Windows:    check Device Manager → Ports (COM & LPT)
 */

const { SerialPort }    = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const { WebSocketServer } = require('ws');

const SERIAL_PORT = process.env.PORT || '/dev/tty.usbserial-58DD0266871';
const BAUD_RATE   = 115200;
const WS_PORT     = 8080;

const port   = new SerialPort({ path: SERIAL_PORT, baudRate: BAUD_RATE });
const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

port.on('open',  ()  => console.log(`✓ Serial open: ${SERIAL_PORT}`));
port.on('error', err => console.error('✗ Serial error:', err.message));

const wss = new WebSocketServer({ port: WS_PORT });
console.log(`✓ WebSocket listening on ws://localhost:${WS_PORT}`);

wss.on('connection', ws => {
  console.log('  Browser connected');
  ws.on('close', () => console.log('  Browser disconnected'));
});

function broadcast(data) {
  wss.clients.forEach(c => { if (c.readyState === 1) c.send(data); });
}

parser.on('data', line => {
  line = line.trim();
  if (!line) return;
  try { JSON.parse(line); broadcast(line); } catch (_) {}
});

process.on('SIGINT', () => { port.close(); wss.close(); process.exit(); });