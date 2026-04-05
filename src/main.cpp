/*
 * Mood Piano — ESP32 TTGO T-Display
 * 7 capacitive touch keys (C D E F G A B)
 * Mode switch = onboard button GPIO 0 (hold 1.5s)
 * Output: Serial JSON → laptop generative engine
 *
 * Pin mapping (all free touch-capable GPIOs on TTGO T-Display):
 *
 *   Key  Note  GPIO  Touch Pin
 *    0    C      2    T2
 *    1    D     13    T4
 *    2    E     12    T5
 *    3    F     14    T6
 *    4    G     27    T7
 *    5    A     33    T9
 *    6    B     32    T8
 *
 *   Mode switch: GPIO 0 (onboard button, already on the board — no extra wiring)
 *
 * Pins NOT available on TTGO T-Display (already used internally):
 *   4, 5, 16, 18, 19, 23 → TFT screen
 *   0, 35                → onboard buttons
 *
 * Wiring:
 *   Copper tape pad → GPIO pin directly
 *   Optional: 1MΩ resistor from each GPIO to GND improves stability
 *
 * Libraries (install via PlatformIO / Arduino Library Manager):
 *   - TFT_eSPI   (configured for TTGO T-Display via platformio.ini flags)
 *   - ArduinoJson
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

// ─── Pin definitions ──────────────────────────────────────────────────────────
const int NUM_KEYS = 7;

const int TOUCH_PINS[NUM_KEYS] = {
  2,   // 0  C  (T2)
  13,  // 1  D  (T4)
  12,  // 2  E  (T5)
  15,  // 3  F  (T6)
  27,  // 4  G  (T7)
  33,  // 5  A  (T9)
  32,  // 6  B  (T8)
};

const char* KEY_NAMES[NUM_KEYS] = {"C", "D", "E", "F", "G", "A", "B"};

// GPIO 0 = onboard button on TTGO T-Display (active LOW, has internal pullup)
const int MODE_BTN = 0;

// ─── Tuning ───────────────────────────────────────────────────────────────────
const int  TOUCH_THRESHOLD  = 40;   // raw value below this = finger detected
                                     // tune if keys mis-fire: raise threshold
                                     // tune if keys don't register: lower it
const int  MODE_HOLD_MS     = 1500; // hold button this long to switch mode
const int  SEND_INTERVAL_MS = 50;   // JSON send rate (20hz)
const int  DISP_INTERVAL_MS = 100;  // display refresh rate (10hz)

// ─── State ────────────────────────────────────────────────────────────────────
bool          keyActive[NUM_KEYS]    = {};
bool          keyWasActive[NUM_KEYS] = {};
int           holdDuration[NUM_KEYS] = {};
unsigned long keyPressTime[NUM_KEYS] = {};

int           currentMode    = 1;   // 1 = compose,  2 = direct play
unsigned long modeBtnStart   = 0;
bool          modeBtnArmed   = false;
bool          modeSwitchDone = false;

unsigned long lastSend    = 0;
unsigned long lastDisplay = 0;

TFT_eSPI tft = TFT_eSPI();

// ─── Mood detection ───────────────────────────────────────────────────────────
// Keys map to scale degrees: C D E F G A B
// Combos are chosen for musical meaning on a diatonic scale
String detectMood(bool* a) {
  // 3-key combos (triads)
  if (a[0] && a[2] && a[4]) return "euphoric";    // C E G = C major triad
  if (a[1] && a[3] && a[5]) return "nostalgic";   // D F A = D minor triad
  if (a[2] && a[4] && a[6]) return "resolved";    // E G B = E minor triad
  if (a[3] && a[5] && a[0]) return "dreamy";      // F A C = F major triad
  if (a[4] && a[6] && a[1]) return "urgent";      // G B D = G major triad

  // 2-key combos (intervals)
  if (a[0] && a[4]) return "grounded";     // C + G = perfect fifth, stable
  if (a[0] && a[6]) return "tense";        // C + B = major seventh, unresolved
  if (a[1] && a[5]) return "melancholic";  // D + A = minor feel
  if (a[2] && a[6]) return "mysterious";   // E + B = open, floating
  if (a[3] && a[6]) return "ethereal";     // F + B = tritone, suspended
  if (a[0] && a[3]) return "calm";         // C + F = perfect fourth, restful
  if (a[4] && a[0]) return "hopeful";      // G + C = resolution

  // Single keys
  if (a[0]) return "neutral";
  if (a[1]) return "melancholic";
  if (a[2]) return "playful";
  if (a[3]) return "calm";
  if (a[4]) return "grounded";
  if (a[5]) return "wistful";
  if (a[6]) return "tense";

  return "neutral";
}

// ─── Mood → display color ─────────────────────────────────────────────────────
uint16_t moodColor(String mood) {
  if (mood == "euphoric"   || mood == "hopeful")    return TFT_GREEN;
  if (mood == "nostalgic"  || mood == "melancholic") return 0x839F;  // soft purple
  if (mood == "tense"      || mood == "urgent")      return TFT_RED;
  if (mood == "serene"     || mood == "ethereal")    return TFT_CYAN;
  if (mood == "playful"    || mood == "resolved")    return 0x07E0;  // bright green
  if (mood == "calm"       || mood == "grounded")    return 0x07FF;  // teal
  if (mood == "mysterious" || mood == "dreamy")      return 0xC81F;  // magenta
  if (mood == "wistful")                             return 0xFD20;  // orange
  return TFT_WHITE;
}

// ─── Draw TFT display ─────────────────────────────────────────────────────────
void drawDisplay(String mood) {
  tft.fillScreen(TFT_BLACK);

  // Mode indicator — top right
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(168, 6);
  tft.print(currentMode == 1 ? "COMPOSE" : "DIRECT");

  // Title — top left
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(6, 6);
  tft.print("mood piano");

  // Current mood — large colored text
  uint16_t mc = moodColor(mood);
  tft.setTextColor(mc);
  tft.setTextSize(2);
  tft.setCursor(6, 32);
  tft.print(mood);

  // Mini keyboard — 7 white keys
  const int y0  = 84;
  const int kW  = 26, kH = 44;
  const int gap = 3;

  for (int i = 0; i < NUM_KEYS; i++) {
    int x = 6 + i * (kW + gap);
    bool on = keyActive[i];
    // key body
    if (on) {
      tft.fillRect(x, y0, kW, kH, mc);
    } else {
      tft.drawRect(x, y0, kW, kH, 0x4208); // dim gray outline
    }
    // note label inside key
    tft.setTextSize(1);
    tft.setTextColor(on ? TFT_BLACK : 0x4208);
    tft.setCursor(x + 8, y0 + kH - 12);
    tft.print(KEY_NAMES[i]);
  }

  // Bottom hint
  tft.setTextSize(1);
  tft.setTextColor(0x2104);
  tft.setCursor(6, 132);
  tft.print("hold BTN0 1.5s = switch mode");
}

// ─── Send JSON over Serial ────────────────────────────────────────────────────
void sendJSON(String mood) {
  StaticJsonDocument<256> doc;
  doc["mode"] = currentMode;
  doc["mood"] = mood;

  JsonArray keys = doc.createNestedArray("keys");
  JsonArray hold = doc.createNestedArray("hold");
  for (int i = 0; i < NUM_KEYS; i++) {
    keys.add(keyActive[i] ? 1 : 0);
    hold.add(keyActive[i] ? holdDuration[i] : 0);
  }

  serializeJson(doc, Serial);
  Serial.println(); // newline = message delimiter on laptop side
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // GPIO 0 is the onboard button — active LOW, internal pullup
  pinMode(MODE_BTN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1); // landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 48);
  tft.print("mood piano");
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(20, 78);
  tft.print("touch a key to begin");
  tft.setCursor(20, 96);
  tft.print("hold BTN0 to switch mode");
  delay(1000);
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Read all 7 capacitive touch keys ───────────────────────────────────────
  for (int i = 0; i < NUM_KEYS; i++) {
    bool touched = (touchRead(TOUCH_PINS[i]) < TOUCH_THRESHOLD);
    keyActive[i] = touched;

    if (touched) {
      if (!keyWasActive[i]) keyPressTime[i] = now; // fresh press
      holdDuration[i] = (int)(now - keyPressTime[i]);
    } else {
      holdDuration[i] = 0;
    }
    keyWasActive[i] = touched;
  }

  // ── Mode button (GPIO 0, onboard) — hold 1.5s to toggle ───────────────────
  bool btnHeld = (digitalRead(MODE_BTN) == LOW);
  if (btnHeld) {
    if (!modeBtnArmed) {
      modeBtnArmed   = true;
      modeBtnStart   = now;
      modeSwitchDone = false;
    }
    if (!modeSwitchDone && (now - modeBtnStart) >= MODE_HOLD_MS) {
      currentMode    = (currentMode == 1) ? 2 : 1;
      modeSwitchDone = true;
      // flash confirmation on display
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.setCursor(14, 56);
      tft.print(currentMode == 1 ? "COMPOSE MODE" : "DIRECT  MODE");
      delay(600);
    }
  } else {
    modeBtnArmed   = false;
    modeSwitchDone = false;
  }

  // ── Transmit JSON + refresh display ────────────────────────────────────────
  if ((now - lastSend) >= SEND_INTERVAL_MS) {
    String mood = detectMood(keyActive);
    sendJSON(mood);
    lastSend = now;

    if ((now - lastDisplay) >= DISP_INTERVAL_MS) {
      drawDisplay(mood);
      lastDisplay = now;
    }
  }
}