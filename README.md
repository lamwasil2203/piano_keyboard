# Mood Piano

A capacitive-touch instrument built on an ESP32 TTGO T-Display. Touch copper-tape keys to trigger moods; a generative music engine on the laptop responds in real time with synthesized sound and a live particle visualization.

## How it works

```
Copper keys → ESP32 → Serial JSON → Node.js bridge → WebSocket → Browser
```

1. **Firmware** reads 7 capacitive touch pads (C D E F G A B), detects the musical mood from the combination of keys held, and sends a JSON packet over USB serial at 20 Hz.
2. **Serial bridge** (`serial_bridge.js`) receives the JSON and rebroadcasts it over a local WebSocket.
3. **Browser UI** (`index.html`) connects to the WebSocket, applies the mood to a generative sequencer, and renders a canvas particle visualization.

## Hardware

| Part | Details |
|------|---------|
| Board | LILYGO TTGO T-Display (ESP32, ST7789 135×240 TFT) |
| Keys | Copper tape pads wired to GPIO touch pins |
| Extras | Optional 1 MΩ resistor per pin to GND for stability |

### Pin mapping

| Key | Note | GPIO | Touch pin |
|-----|------|------|-----------|
| 0 | C | 2 | T2 |
| 1 | D | 13 | T4 |
| 2 | E | 12 | T5 |
| 3 | F | 15 | T6 |
| 4 | G | 27 | T7 |
| 5 | A | 33 | T9 |
| 6 | B | 32 | T8 |

Mode switch: **GPIO 0** (onboard button, no extra wiring needed).

## Modes

| Mode | Behavior |
|------|----------|
| **Compose** (1) | Generative sequencer runs; mood controls BPM, scale, note density, reverb, and waveform. |
| **Direct** (2) | Each key plays and sustains its note for as long as it is held. |

Hold the onboard **BTN0** for 1.5 s to toggle between modes. The current mode flashes on the TFT and is shown in the browser UI.

## Moods

Moods are determined by which keys are pressed simultaneously.

| Keys | Mood |
|------|------|
| C + E + G | euphoric |
| D + F + A | nostalgic |
| E + G + B | resolved |
| F + A + C | dreamy |
| G + B + D | urgent |
| C + G | grounded |
| C + B | tense |
| D + A | melancholic |
| E + B | mysterious |
| F + B | ethereal |
| C + F | calm |
| C (solo) | neutral |
| D (solo) | melancholic |
| E (solo) | playful |
| F (solo) | calm |
| G (solo) | grounded |
| A (solo) | wistful |
| B (solo) | tense |

Each mood carries a set of parameters (tension, warmth, space, motion, darkness, BPM, scale) that drive the generative engine.

## Project structure

```
Keyboard/
├── src/
│   └── main.cpp          # ESP32 firmware
├── piano_laptop/
│   ├── serial_bridge.js  # Node.js Serial → WebSocket bridge
│   ├── index.html        # Browser UI / generative engine
│   └── package.json
└── platformio.ini        # PlatformIO build config (TTGO T-Display + TFT_eSPI)
```

## Setup

### 1. Flash the firmware

Open the project in PlatformIO (VS Code extension or CLI) and upload to the TTGO T-Display:

```bash
pio run --target upload
```

Monitor serial output:

```bash
pio device monitor --baud 115200
```

### 2. Run the serial bridge

```bash
cd piano_laptop
npm install
```

Find the correct serial port:

```bash
npm run find-port
```

Set the port if it differs from the default, then start the bridge:

```bash
PORT=/dev/tty.usbserial-XXXXXXXX npm start
```

The bridge listens for WebSocket connections on `ws://localhost:8080`.

### 3. Open the browser UI

Open `piano_laptop/index.html` directly in a browser (no server needed). Click **"click to begin"** to unlock the Web Audio API, then touch the keys.

## Dependencies

### Firmware (installed automatically by PlatformIO)

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) `^2.5.0`
- [ArduinoJson](https://arduinojson.org/) `^6.21.0`

### Laptop bridge

- [serialport](https://serialport.io/) `^12.0.0`
- [ws](https://github.com/websockets/ws) `^8.14.0`

## Serial JSON format

The firmware emits one JSON line per tick (50 ms interval):

```json
{
  "mode": 1,
  "mood": "euphoric",
  "keys": [1, 0, 1, 0, 1, 0, 0],
  "hold": [320, 0, 180, 0, 95, 0, 0]
}
```

- `mode` — 1 = Compose, 2 = Direct
- `mood` — detected mood string
- `keys` — 1/0 active state for each of the 7 keys (C→B)
- `hold` — milliseconds each key has been held (0 if not active)

## Tuning

Adjust these constants at the top of `src/main.cpp` if touch sensitivity needs calibration:

| Constant | Default | Effect |
|----------|---------|--------|
| `TOUCH_THRESHOLD` | 40 | Lower = less sensitive; raise if keys mis-fire |
| `MODE_HOLD_MS` | 1500 ms | Duration required to switch mode |
| `SEND_INTERVAL_MS` | 50 ms | Serial transmit rate (20 Hz) |
| `DISP_INTERVAL_MS` | 100 ms | TFT refresh rate (10 Hz) |
