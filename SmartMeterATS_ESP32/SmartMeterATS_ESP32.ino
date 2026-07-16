/*
 * ============================================================
 *  Smart Automatic Energy Meter Switching System
 *  UNIVERSAL FIRMWARE — supports 1..10 meters, ONE binary
 *  Board  : ESP32 DevKit (WROOM-32)
 *  Author : Eng-Nabeel
 *  Date   : 2026
 *
 *  ARCHITECTURE
 *  ------------
 *  Every customer receives EXACTLY this firmware binary.
 *  The PCB carries footprints for 10 relay modules; only the
 *  purchased number of relay modules is populated. The single
 *  runtime difference between installations is the "Active
 *  Meters" value (1-10) chosen once on the dashboard and
 *  persisted in NVS. No recompilation, ever.
 *
 *  All relay GPIOs are permanently assigned in the table below
 *  and are NEVER exposed to or configurable from the dashboard.
 *
 *  WIRING SUMMARY (fixed, all 10 positions)
 *  ----------------------------------------
 *  Relay1  -> GPIO25      Relay6  -> GPIO13
 *  Relay2  -> GPIO26      Relay7  -> GPIO4
 *  Relay3  -> GPIO27      Relay8  -> GPIO18
 *  Relay4  -> GPIO14      Relay9  -> GPIO19
 *  Relay5  -> GPIO32      Relay10 -> GPIO23
 *  PZEM RX(ESP)  -> GPIO16 (UART2 RX, PZEM TX -> ESP GPIO16)
 *  PZEM TX(ESP)  -> GPIO17 (UART2 TX, ESP GPIO17 -> PZEM RX)
 *  RTC SDA       -> GPIO21 (default I2C SDA)
 *  RTC SCL       -> GPIO22 (default I2C SCL)
 *  Btn EmergOFF  -> GPIO33 (INPUT_PULLUP, button to GND)
 *
 *  All 10 relay GPIOs are output-capable, non-strapping pins
 *  that don't collide with UART0/UART2, I2C or flash pins.
 *  Unpopulated relay positions are simply driven OFF forever.
 *
 *  PZEM-004T runs on hardware UART2 (Serial2) — no SoftwareSerial.
 *  Settings persist in NVS via Preferences.h — no EEPROM emulation.
 *
 *  WiFi Access Point
 *  SSID: SmartMeterATS   Password: 12345678
 *  Dashboard: http://192.168.4.1/
 *
 *  REQUIRED LIBRARIES (install via Arduino Library Manager)
 *  - PZEM004Tv30  by Jakub Mandula (v1.1.2+, ESP32 hardware serial support)
 *  - RTClib       by Adafruit
 *  - ArduinoJson  by Benoit Blanchon
 *  (WiFi, WebServer, Preferences bundled with ESP32 Arduino core)
 * ============================================================
 */

// ============================================================
//  INCLUDES
// ============================================================
#include <WiFi.h>
#include <WebServer.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <RTClib.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

// ============================================================
//  PIN DEFINITIONS
//  RELAY_PINS is the ONLY place GPIO<->meter mapping exists.
//  It is fixed for the life of the product. The dashboard and
//  API never see GPIO numbers — only meter indices 0..N-1.
// ============================================================
#define MAX_METERS  10   // PCB supports up to 10 relay positions

const uint8_t RELAY_PINS[MAX_METERS] = {
  25,  // Meter 1
  26,  // Meter 2
  27,  // Meter 3
  14,  // Meter 4
  32,  // Meter 5
  13,  // Meter 6
   4,  // Meter 7
  18,  // Meter 8
  19,  // Meter 9
  23   // Meter 10
};

#define PZEM_RX_PIN     16  // UART2 RX (receives from PZEM TX)
#define PZEM_TX_PIN     17  // UART2 TX (sends to PZEM RX)
#define I2C_SDA_PIN     21  // ESP32 default SDA
#define I2C_SCL_PIN     22  // ESP32 default SCL
#define BTN_EMERGENCY   33  // input-capable with internal pull-up, no strapping function

// Relay logic: most relay modules are ACTIVE LOW
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

#define DEFAULT_ACTIVE_METERS  3   // sensible first-boot default

// ============================================================
//  WIFI ACCESS POINT
// ============================================================
const char* AP_SSID     = "SmartMeterATS";
const char* AP_PASSWORD = "12345678";
IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

// ============================================================
//  NVS STORAGE (Preferences.h)
//  Namespace "smartats" — named keys, all sized for MAX_METERS
//  so the same layout works for every customer.
// ============================================================
#define NVS_NAMESPACE   "smartats"
// Keys:
//   mcount       uchar   Active Meters (1..MAX_METERS)
//   lim0..lim9   float   per-meter kWh limits
//   en0..en9     bool    per-meter enabled flags
//   active       uchar   active meter index
//   daily        bytes   float[30] daily usage ring buffer
//   didx         uchar   rolling daily write index
//   today        float   today kWh
//   curmon       float   current month kWh
//   lastmon      float   last month kWh
//   ov, uv, oc   float   protection thresholds
//   emerg        bool    emergency-off latched state   (BUG FIX #8)
//   bypass       bool    bypass mode flag              (BUG FIX #2)
//   rstmon       uchar   month of last monthly reset   (BUG FIX #3)
//   rstyr        ushort  year  of last monthly reset   (BUG FIX #3)

// ============================================================
//  TIMING CONSTANTS (ms)
// ============================================================
#define RELAY_SWITCH_DELAY   500
#define PZEM_READ_INTERVAL   2000
#define SERIAL_PRINT_INTERVAL 3000
#define BTN_DEBOUNCE_MS      50
#define WDT_TIMEOUT_S        10     // task watchdog timeout (seconds)

// ============================================================
//  OBJECTS
// ============================================================
// PZEM on hardware UART2. The PZEM004Tv30 ESP32 constructor calls
// Serial2.begin(9600, SERIAL_8N1, RX, TX) itself — no manual begin().
PZEM004Tv30          pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
RTC_DS3231           rtc;
WebServer            server(80);
Preferences          prefs;

// ============================================================
//  SYSTEM STATE
// ============================================================
struct MeterConfig {
  float energyLimit;   // kWh limit set by user
  bool  enabled;       // is this meter enabled?
  float energyUsed;    // accumulated kWh for this meter (session)
};

MeterConfig meters[MAX_METERS];

// Runtime meter count (1..MAX_METERS). Set once on the dashboard,
// persisted in NVS. Everything — relay logic, JSON, scheduling,
// protection restore — loops over this value, never over a literal.
uint8_t activeMeterCount = DEFAULT_ACTIVE_METERS;

int  activeMeter     = 0;     // 0-indexed active meter
bool emergencyOff    = false; // all relays OFF (persisted — BUG FIX #8)
// BUG FIX #2: bypass mode — automatic limit-based switching is
// disabled, manual switching still works. Persisted in NVS.
bool bypassMode      = false;
int  lastDay         = -1;    // for monthly reset detection
bool rtcOK           = false; // DS3231 detected once at boot

// Live PZEM readings
float liveVoltage    = 0.0f;
float liveCurrent    = 0.0f;
float livePower      = 0.0f;
float liveEnergy     = 0.0f;    // kWh from PZEM (reset-able)
float pzemEnergyBase = 0.0f;    // baseline for per-meter tracking
bool  pzemOK         = false;   // true only when PZEM returns valid data

// Daily / monthly statistics
float todayUsed          = 0.0f;
float currentMonthUsed   = 0.0f;
float lastMonthUsed      = 0.0f;
float dailyUsage[30]     = {0};
int   dailyIndex         = 0;
float prevLiveEnergy     = -1.0f; // -1 = not yet initialised
int   lastTrackedDay     = -1;
int   lastTrackedMonth   = -1;
// BUG FIX #3: (year,month) of the last monthly reset, persisted in
// NVS so a reset missed while powered off is caught at boot.
int   lastResetMonth     = -1;    // -1/0 = never reset yet
int   lastResetYear      = -1;
// BUG FIX #5: timestamp of the last valid energy sample, used to
// estimate energy lost across a PZEM counter reset.
unsigned long lastEnergySampleMs = 0;

// Protection thresholds & fault state
float ovVoltThresh   = 250.0f;  // V  over-voltage limit
float uvVoltThresh   = 180.0f;  // V  under-voltage limit
float ocCurrThresh   = 16.0f;   // A  over-current limit
bool  protTrip       = false;   // latched protection trip
String protReason    = "";      // human-readable trip reason

// Non-blocking meter switch state machine
//   pendingMeter == -1 : idle
//   pendingMeter >=  0 : relays are OFF, waiting RELAY_SWITCH_DELAY,
//                        then that meter's relay turns ON
int           pendingMeter  = -1;
unsigned long switchOffTime = 0;

// ------------------------------------------------------------
//  DUAL-CORE CONCURRENCY
//  pzemTask (Core 0) runs readEnergyData() + handleProtection().
//  loop()   (Core 1) runs web server, buttons, relay logic, resets.
//  Every access to the shared state above (live readings, meters[],
//  activeMeterCount, protTrip/protReason, stats, thresholds,
//  activeMeter, pending switch state, ...) must hold stateMux.
//  Recursive so functions that lock (e.g. switchToMeter) can be
//  called from sections that already hold it. Keep critical
//  sections short — never hold the mutex across pzem.*() Modbus
//  reads or NVS writes.
//  I2C (RTC) is only ever touched from Core 1, so Wire needs no lock.
// ------------------------------------------------------------
SemaphoreHandle_t stateMux = NULL;
TaskHandle_t      pzemTaskHandle = NULL;
#define STATE_LOCK()   xSemaphoreTakeRecursive(stateMux, portMAX_DELAY)
#define STATE_UNLOCK() xSemaphoreGiveRecursive(stateMux)

// Timestamps (non-blocking)
unsigned long lastSerialPrint = 0;

// BUG FIX #7: NVS write throttling. Routine changes only set the
// dirty flag; handleDeferredSave() flushes at most once per 30s to
// protect flash endurance (~100K erase cycles). Critical events
// (emergency stop, protection trip) call saveSettings() directly
// for an immediate write.
volatile bool settingsDirty = false;
unsigned long lastNvsWrite  = 0;
#define NVS_MIN_WRITE_INTERVAL_MS  30000UL

// Button state tracking
bool          btnEmergLast  = HIGH;
unsigned long btnEmergTime  = 0;

// ============================================================
//  FUNCTION PROTOTYPES
// ============================================================
void setupWiFi();
void setupPins();
void readEnergyData();
void handleRelayLogic();
void handleButtons();
void handleTimeBasedResets();
void saveSettings();
void loadSettings();
void switchToMeter(int meterIndex);
void handlePendingSwitch();
void allRelaysOff();
int  nextEnabledMeter(int current);
int  firstEnabledMeter();
void printSerial();
void handleProtection();
void handleApiStatus();
void handleApiSetLimits();
void handleApiSetEnabled();
void handleApiSwitchMeter();
void handleApiResetEnergy();
void handleApiEmergency();
void handleApiSetProtection();
void handleApiClearFault();
void handleApiSetActiveMeters();
void handleApiSetBypass();
void performMonthlyReset(const DateTime& now);
void handleDeferredSave();
void pzemTask(void* param);

// ============================================================
//  EMBEDDED HTML DASHBOARD
//  The page contains NO per-meter markup. JavaScript reads
//  meterCount from /api/status and generates every meter card,
//  limit row and dropdown option dynamically. GPIO numbers are
//  never sent to the browser.
// ============================================================
const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Smart Meter ATS</title>
<style>
:root{--bg:#080f1d;--card:#0f1e35;--card2:#0d1f38;--border:#1a3050;--text:#c8d8ed;--muted:#3d5a7a;--accent:#3b82f6;--cyan:#06b6d4;--green:#22c55e;--yellow:#eab308;--red:#ef4444;--orange:#f97316}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
.header{background:linear-gradient(135deg,#050d1a,#0a1a30,#050d1a);border-bottom:1px solid var(--border);padding:14px 24px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px}
.htitle{display:flex;align-items:center;gap:12px}
.hlogo{font-size:2rem;filter:drop-shadow(0 0 12px #3b82f6)}
.htitle h1{font-size:1.35rem;font-weight:700;color:#fff;letter-spacing:-0.02em}
.htitle p{font-size:0.74rem;color:var(--muted);margin-top:2px}
.badges{display:flex;gap:8px;flex-wrap:wrap}
.badge{display:inline-flex;align-items:center;gap:5px;padding:4px 11px;border-radius:20px;font-size:0.72rem;font-weight:700;border:1px solid transparent;letter-spacing:.03em}
.bdg-conn{background:#031a0a;border-color:#14532d;color:#86efac}
.bdg-conn.off{background:#1f0808;border-color:#7f1d1d;color:#fca5a5}
.bdg-mode{background:#0f1235;border-color:#1e3a8a;color:#93c5fd}
.bdg-mode.bypass{background:#1c1100;border-color:#78350f;color:#fde68a}
.bdg-mode.emg{background:#1f0808;border-color:#7f1d1d;color:#fca5a5;animation:pulseR 1s infinite}
.dot{width:7px;height:7px;border-radius:50%;background:currentColor}
@keyframes pulseR{0%,100%{opacity:1}50%{opacity:.4}}
.main{padding:18px;max-width:1020px;margin:0 auto}
.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:14px}
@media(max-width:680px){.metrics{grid-template-columns:repeat(2,1fr)}}
.mc{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:16px 18px;position:relative;overflow:hidden;transition:transform .2s}
.mc:hover{transform:translateY(-2px)}
.mc::before{content:'';position:absolute;top:0;left:0;right:0;height:3px}
.mc.v::before{background:linear-gradient(90deg,#3b82f6,#93c5fd)}
.mc.a::before{background:linear-gradient(90deg,#06b6d4,#67e8f9)}
.mc.w::before{background:linear-gradient(90deg,#f97316,#fbbf24)}
.mc.e::before{background:linear-gradient(90deg,#22c55e,#86efac)}
.mc-icon{font-size:1.3rem;margin-bottom:6px;opacity:.75}
.mc-val{font-size:1.75rem;font-weight:700;color:#fff;line-height:1.1}
.mc-val.na{color:var(--muted);font-size:1.4rem}
.mc-unit{font-size:0.72rem;color:var(--muted);font-weight:600;margin-top:1px}
.mc-lbl{font-size:0.68rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;margin-top:5px}
.content{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media(max-width:680px){.content{grid-template-columns:1fr}}
.card{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:20px}
.ctitle{font-size:0.72rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);font-weight:700;margin-bottom:14px;display:flex;align-items:center;gap:6px}
.meter-list{display:flex;flex-direction:column;gap:9px}
.mi{background:var(--card2);border:1px solid var(--border);border-radius:10px;padding:11px 14px;transition:all .3s}
.mi.on{border-color:var(--accent);background:#091828;box-shadow:0 0 18px rgba(59,130,246,.12)}
.mi.dim{opacity:.4}
.mi-top{display:flex;align-items:center;justify-content:space-between;margin-bottom:7px}
.mi-name{font-weight:600;font-size:0.88rem;color:#fff;display:flex;align-items:center;gap:7px}
.adot{width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 8px var(--accent);animation:blink 1.5s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.mt{font-size:0.68rem;padding:2px 8px;border-radius:10px;font-weight:700}
.mt.act{background:#0c2a50;color:#93c5fd}.mt.stby{background:#0c2a50;color:#64748b}.mt.dis{background:#1a1a1a;color:#4b5563}
.bwrap{background:#050e1c;border-radius:6px;height:6px;overflow:hidden}
.bfill{height:100%;border-radius:6px;transition:width .7s ease;background:linear-gradient(90deg,var(--accent),var(--cyan))}
.bfill.warn{background:linear-gradient(90deg,var(--orange),var(--yellow))}
.bfill.danger{background:linear-gradient(90deg,var(--red),var(--orange))}
.mstats{display:flex;justify-content:space-between;font-size:0.7rem;color:var(--muted);margin-top:4px}
.lrow{margin-bottom:11px}
.lhdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:5px}
.llbl{font-size:0.82rem;color:var(--text);font-weight:500}
.toggle{position:relative;width:38px;height:21px;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:#0d1e35;border-radius:21px;cursor:pointer;transition:.3s;border:1px solid var(--border)}
.slider:before{content:'';position:absolute;width:15px;height:15px;left:2px;top:2px;background:#2d4a6a;border-radius:50%;transition:.3s}
input:checked+.slider{background:#1d4ed8;border-color:#3b82f6}
input:checked+.slider:before{transform:translateX(17px);background:#fff}
.linp{width:100%;padding:8px 11px;background:#040b16;border:1px solid var(--border);color:#fff;border-radius:8px;font-size:0.88rem;outline:none;transition:border .2s}
.linp:focus{border-color:var(--accent)}
select.csel{width:100%;padding:9px 11px;background:#040b16;border:1px solid var(--border);color:#fff;border-radius:8px;font-size:0.88rem;outline:none;cursor:pointer}
.btn{width:100%;padding:10px;border:none;border-radius:9px;font-size:0.88rem;font-weight:700;cursor:pointer;transition:all .2s;margin-top:8px;display:flex;align-items:center;justify-content:center;gap:6px;letter-spacing:.01em}
.btn:hover{transform:translateY(-1px);filter:brightness(1.1)}
.btn:active{transform:translateY(0)}
.btn-blue{background:linear-gradient(135deg,#1e40af,#2563eb);color:#fff;box-shadow:0 4px 14px rgba(37,99,235,.25)}
.btn-green{background:linear-gradient(135deg,#065f46,#059669);color:#fff;box-shadow:0 4px 14px rgba(5,150,105,.2)}
.btn-amber{background:linear-gradient(135deg,#92400e,#d97706);color:#fff;box-shadow:0 4px 14px rgba(217,119,6,.2)}
.btn-red{background:linear-gradient(135deg,#991b1b,#dc2626);color:#fff;box-shadow:0 4px 14px rgba(220,38,38,.25)}
.ctrls{display:grid;grid-template-columns:repeat(auto-fit,minmax(175px,1fr));gap:10px;align-items:start}
.cg{}
.cg-lbl{font-size:0.75rem;color:var(--muted);margin-bottom:5px}
.hint{font-size:0.7rem;color:var(--muted);margin-top:8px;line-height:1.5}
#toast{position:fixed;bottom:20px;right:20px;padding:10px 16px;border-radius:10px;font-size:0.82rem;font-weight:600;color:#fff;opacity:0;transform:translateY(8px);transition:all .3s;z-index:999;pointer-events:none;max-width:260px;border:1px solid transparent}
#toast.show{opacity:1;transform:translateY(0)}
#toast.ok{background:#052e16;border-color:#166534}
#toast.warn{background:#422006;border-color:#92400e}
#toast.err{background:#450a0a;border-color:#991b1b}
.htitle>div>p{color:#3b82f6!important;font-weight:700;letter-spacing:.05em}
.stats-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}
@media(max-width:600px){.stats-grid{grid-template-columns:repeat(2,1fr)}}
.stat-item{background:var(--card2);border:1px solid var(--border);border-radius:10px;padding:14px;text-align:center}
.stat-icon{font-size:1.4rem;margin-bottom:6px}
.stat-val{font-size:1.35rem;font-weight:700;color:#fff}
.stat-unit{font-size:0.7rem;color:var(--muted);font-weight:600;margin-top:1px}
.stat-lbl{font-size:0.68rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;margin-top:4px}
.graph-wrap{display:flex;align-items:flex-end;gap:3px;height:90px;border-bottom:1px solid var(--border);margin-bottom:4px}
.gb{flex:1;min-width:0;border-radius:3px 3px 0 0;background:linear-gradient(180deg,#2563eb,#1e3a8a);position:relative;transition:height .5s ease;cursor:pointer}
.gb.today{background:linear-gradient(180deg,var(--cyan),#0e7490)}
.gb:hover::after{content:attr(data-v);position:absolute;top:-22px;left:50%;transform:translateX(-50%);font-size:0.6rem;color:#fff;background:#1e293b;padding:2px 5px;border-radius:4px;white-space:nowrap;pointer-events:none;z-index:10}
.graph-lbl{display:flex;justify-content:space-between;font-size:0.62rem;color:var(--muted);padding:0 2px}
.footer{text-align:center;padding:12px;font-size:0.69rem;color:var(--muted);border-top:1px solid var(--border);margin-top:16px}
.bdg-mode.fault{background:#2d0a0a;border-color:#dc2626;color:#fca5a5;animation:pulseR .8s infinite}
.fault-banner{background:linear-gradient(135deg,#3b0101,#600000);border:1px solid #dc2626;border-radius:12px;padding:14px 18px;margin-bottom:14px;display:none;align-items:center;gap:14px;flex-wrap:wrap}
.fault-banner.show{display:flex}
.fb-icon{font-size:1.8rem;flex-shrink:0}
.fb-body{flex:1;min-width:160px}
.fb-title{font-size:0.88rem;font-weight:700;color:#fca5a5;margin-bottom:3px}
.fb-reason{font-size:0.76rem;color:#fca5a5;opacity:.8}
.prot-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(175px,1fr));gap:10px}
</style>
</head>
<body>
<header class="header">
  <div class="htitle">
    <div class="hlogo">&#9889;</div>
    <div>
      <h1>Smart Meter ATS</h1>
      <p style="color:#3b82f6;font-weight:700;letter-spacing:.05em">Eng-Nabeel</p>
    </div>
  </div>
  <div class="badges">
    <span class="badge bdg-conn off" id="bdgConn"><span class="dot"></span><span id="connTxt">Connecting</span></span>
    <span class="badge bdg-mode" id="bdgMode"><span class="dot"></span><span id="modeTxt">AUTO</span></span>
  </div>
</header>
<main class="main">
<div class="fault-banner" id="faultBanner">
  <div class="fb-icon">&#9889;</div>
  <div class="fb-body">
    <div class="fb-title">&#9888; PROTECTION TRIP &mdash; All Relays OFF</div>
    <div class="fb-reason" id="faultReason">Unknown fault</div>
  </div>
  <button class="btn btn-red" style="width:auto;margin-top:0;padding:8px 16px;flex-shrink:0" onclick="clearFault()">&#10003; Clear Fault</button>
</div>
  <div class="metrics">
    <div class="mc v"><div class="mc-icon">&#128268;</div><div class="mc-val na" id="mv">--</div><div class="mc-unit">Volts</div><div class="mc-lbl">Voltage</div></div>
    <div class="mc a"><div class="mc-icon">&#x26A1;</div><div class="mc-val na" id="ma">--</div><div class="mc-unit">Ampere</div><div class="mc-lbl">Current</div></div>
    <div class="mc w"><div class="mc-icon">&#128161;</div><div class="mc-val na" id="mw">--</div><div class="mc-unit">Watts</div><div class="mc-lbl">Power</div></div>
    <div class="mc e"><div class="mc-icon">&#128200;</div><div class="mc-val na" id="me">--</div><div class="mc-unit">kWh</div><div class="mc-lbl">Energy</div></div>
  </div>
  <div class="content">
    <div class="card">
      <div class="ctitle">&#9648; Meter Status</div>
      <div class="meter-list" id="meterList"></div>
    </div>
    <div class="card">
      <div class="ctitle">&#9881; Energy Limits</div>
      <div id="limitRows"></div>
      <button class="btn btn-blue" onclick="saveLimits()">&#128190; Save Limits</button>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128200; Energy Statistics</div>
      <div class="stats-grid">
        <div class="stat-item"><div class="stat-icon">&#9728;</div><div class="stat-val" id="stToday">0.000</div><div class="stat-unit">kWh</div><div class="stat-lbl">Today</div></div>
        <div class="stat-item"><div class="stat-icon">&#128197;</div><div class="stat-val" id="stMonth">0.000</div><div class="stat-unit">kWh</div><div class="stat-lbl">This Month</div></div>
        <div class="stat-item"><div class="stat-icon">&#128336;</div><div class="stat-val" id="stLastMon">0.000</div><div class="stat-unit">kWh</div><div class="stat-lbl">Last Month</div></div>
        <div class="stat-item"><div class="stat-icon">&#128198;</div><div class="stat-val" id="stNext" style="font-size:1rem">--</div><div class="stat-unit"></div><div class="stat-lbl">Next Reset</div></div>
      </div>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128202; 30-Day Usage (kWh)</div>
      <div class="graph-wrap" id="graph30"></div>
      <div class="graph-lbl"><span>30 days ago</span><span>Today</span></div>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128736; Manual Controls</div>
      <div class="ctrls">
        <div class="cg"><div class="cg-lbl">Switch to Meter</div><select class="csel" id="manM"></select><button class="btn btn-green" onclick="manSwitch()">&#8594; Switch Meter</button></div>
        <div class="cg"><div class="cg-lbl">Energy Counter</div><button class="btn btn-amber" style="margin-top:23px" onclick="resetEnergy()">&#128259; Reset Energy</button></div>
        <div class="cg"><div class="cg-lbl">Bypass Mode</div><button class="btn btn-amber" style="margin-top:23px" id="bypBtn" onclick="toggleBypass()">&#9193; Bypass: OFF</button></div>
        <div class="cg"><div class="cg-lbl">Emergency</div><button class="btn btn-red" style="margin-top:23px" onclick="doEmergency()">&#9888; Emergency OFF</button></div>
      </div>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128295; System Configuration</div>
      <div class="ctrls">
        <div class="cg">
          <div class="cg-lbl">Active Meters</div>
          <select class="csel" id="mcount"></select>
          <button class="btn btn-blue" onclick="saveMeterCount()">&#128190; Apply</button>
        </div>
        <div class="cg">
          <div class="cg-lbl">About</div>
          <p class="hint">Set this to the number of relay modules physically installed on the board (1&ndash;10). The dashboard, scheduler and protection logic automatically adapt. Saved permanently &mdash; no firmware change required.</p>
        </div>
      </div>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128737; Protection Settings</div>
      <div class="prot-grid">
        <div class="cg"><div class="cg-lbl">Over Voltage Threshold (V)</div><input class="linp" type="number" id="pOV" min="220" max="300" step="1" placeholder="250"></div>
        <div class="cg"><div class="cg-lbl">Under Voltage Threshold (V)</div><input class="linp" type="number" id="pUV" min="100" max="220" step="1" placeholder="180"></div>
        <div class="cg"><div class="cg-lbl">Over Current Threshold (A)</div><input class="linp" type="number" id="pOC" min="0.1" max="200" step="0.1" placeholder="16.0"></div>
        <div class="cg"><div class="cg-lbl">Actions</div><button class="btn btn-blue" onclick="saveProtection()">&#128190; Save Thresholds</button><button class="btn btn-amber" style="margin-top:6px" onclick="clearFault()">&#9711; Clear Fault</button></div>
      </div>
    </div>
  </div>
</main>

<div id="toast"></div>
<script>
// N = Active Meters reported by the firmware. Every meter card,
// limit row and dropdown option below is generated from N —
// nothing meter-specific is hard-coded in this page.
var N=0;
var _tt;
function toast(m,t){var e=document.getElementById('toast');e.textContent=m;e.className='show '+(t||'ok');clearTimeout(_tt);_tt=setTimeout(function(){e.className=''},3200);}
function setVal(id,v,d,na){var e=document.getElementById(id);if(na){e.textContent='--';e.className='mc-val na';}else{e.textContent=v.toFixed(d);e.className='mc-val';}}
function buildUI(n){
  N=n;
  var i,h;
  // Meter status cards
  h='';
  for(i=0;i<n;i++){
    h+='<div class="mi" id="mi'+i+'"><div class="mi-top"><div class="mi-name"><span id="ad'+i+'"></span>Meter '+(i+1)+'</div>'
      +'<span class="mt act" id="mt'+i+'">STANDBY</span></div>'
      +'<div class="bwrap"><div class="bfill" id="bar'+i+'" style="width:0%"></div></div>'
      +'<div class="mstats"><span id="us'+i+'">0.000 kWh</span><span id="lm'+i+'">/ 5.0 kWh</span></div></div>';
  }
  document.getElementById('meterList').innerHTML=h;
  // Energy limit rows
  h='';
  for(i=0;i<n;i++){
    h+='<div class="lrow"><div class="lhdr"><span class="llbl">Meter '+(i+1)+' Limit (kWh)</span>'
      +'<label class="toggle"><input type="checkbox" id="en'+i+'" onchange="setEn('+i+')"><span class="slider"></span></label></div>'
      +'<input class="linp" type="number" id="lim'+i+'" min="0.1" step="0.1" placeholder="e.g. 5.0"></div>';
  }
  document.getElementById('limitRows').innerHTML=h;
  // Manual switch dropdown
  h='';
  for(i=0;i<n;i++){h+='<option value="'+i+'">&#128268; Meter '+(i+1)+'</option>';}
  document.getElementById('manM').innerHTML=h;
}
function buildCountSelect(max,cur){
  var s=document.getElementById('mcount');
  if(!s.options.length){
    var h='';
    for(var i=1;i<=max;i++){h+='<option value="'+i+'">'+i+' Meter'+(i>1?'s':'')+'</option>';}
    s.innerHTML=h;
  }
  if(!s.dataset.touched)s.value=cur;
}
function updMeters(d){
  for(var i=0;i<N;i++){
    var mi=document.getElementById('mi'+i);
    var bar=document.getElementById('bar'+i);
    var us=document.getElementById('us'+i);
    var lm=document.getElementById('lm'+i);
    var mt=document.getElementById('mt'+i);
    var ad=document.getElementById('ad'+i);
    var lim=d.limits[i];
    var u=d.used?d.used[i]:0;
    var pct=lim>0?Math.min(100,u/lim*100):0;
    us.textContent=u.toFixed(3)+' kWh';
    lm.textContent='/ '+lim.toFixed(1)+' kWh';
    bar.style.width=pct+'%';
    bar.className='bfill'+(pct>=90?' danger':pct>=70?' warn':'');
    if(!d.enabled[i]){mi.className='mi dim';mt.textContent='DISABLED';mt.className='mt dis';ad.innerHTML='';}
    else if(d.activeMeter===i){mi.className='mi on';mt.textContent='ACTIVE';mt.className='mt act';ad.innerHTML='<span class="adot"></span>';}
    else{mi.className='mi';mt.textContent='STANDBY';mt.className='mt stby';ad.innerHTML='';}
  }
}
function updBadges(d){
  var bc=document.getElementById('bdgConn');
  var ct=document.getElementById('connTxt');
  var bm=document.getElementById('bdgMode');
  var mt=document.getElementById('modeTxt');
  bc.className='badge bdg-conn'+(d.pzemOK?'':' off');
  ct.textContent=d.pzemOK?'PZEM Connected':'PZEM Offline';
  if(d.protTrip){bm.className='badge bdg-mode fault';mt.textContent='FAULT';}
  else if(d.emergency){bm.className='badge bdg-mode emg';mt.textContent='EMERGENCY';}
  else if(d.bypass){bm.className='badge bdg-mode bypass';mt.textContent='BYPASS';}
  else{bm.className='badge bdg-mode';mt.textContent='AUTO';}
}
function updProtection(d){
  var fb=document.getElementById('faultBanner');
  var fr=document.getElementById('faultReason');
  if(d.protTrip){fb.className='fault-banner show';fr.textContent=d.protReason||'Unknown fault';}
  else{fb.className='fault-banner';}
  var ove=document.getElementById('pOV');
  var uve=document.getElementById('pUV');
  var oce=document.getElementById('pOC');
  if(d.ovVolt&&!ove.dataset.loaded){ove.value=d.ovVolt;ove.dataset.loaded='1';}
  if(d.uvVolt&&!uve.dataset.loaded){uve.value=d.uvVolt;uve.dataset.loaded='1';}
  if(d.ocCurr&&!oce.dataset.loaded){oce.value=d.ocCurr;oce.dataset.loaded='1';}
}
async function saveProtection(){
  var ov=parseFloat(document.getElementById('pOV').value)||250;
  var uv=parseFloat(document.getElementById('pUV').value)||180;
  var oc=parseFloat(document.getElementById('pOC').value)||16;
  var r=await fetch('/api/setProtection?ov='+ov+'&uv='+uv+'&oc='+oc);
  var d=await r.json();
  toast(d.status==='ok'?'Protection thresholds saved!':'Error saving thresholds',d.status==='ok'?'ok':'err');
}
async function clearFault(){
  var r=await fetch('/api/clearFault');
  var d=await r.json();
  toast(d.status==='ok'?'Fault cleared — relay restored':'Error clearing fault',d.status==='ok'?'ok':'err');
}
function updStats(d){
  var mn=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  document.getElementById('stToday').textContent=(d.todayUsed||0).toFixed(3);
  document.getElementById('stMonth').textContent=(d.thisMonth||0).toFixed(3);
  document.getElementById('stLastMon').textContent=(d.lastMonth||0).toFixed(3);
  var now=(d.rtcYear&&d.rtcMonth)?new Date(d.rtcYear,d.rtcMonth-1,d.rtcDay||1):new Date();
  var nr=new Date(now.getFullYear(),now.getMonth()+1,1);
  document.getElementById('stNext').textContent=mn[nr.getMonth()]+' 1, '+nr.getFullYear();
  var daily=d.daily&&d.daily.length?d.daily:Array(30).fill(0);
  var mx=Math.max.apply(null,daily)||1;
  var g=document.getElementById('graph30');
  g.innerHTML='';
  for(var i=0;i<daily.length;i++){
    var b=document.createElement('div');
    b.className='gb'+(i===daily.length-1?' today':'');
    b.style.height=Math.max(2,Math.round(daily[i]/mx*80))+'px';
    b.setAttribute('data-v',daily[i].toFixed(3)+' kWh');
    g.appendChild(b);
  }
}
async function poll(){
  try{
    var r=await fetch('/api/status');
    var d=await r.json();
    if(d.meterCount!==N)buildUI(d.meterCount);
    buildCountSelect(d.maxMeters||10,d.meterCount);
    var na=!d.pzemOK;
    setVal('mv',d.voltage,1,na);
    setVal('ma',d.current,2,na);
    setVal('mw',d.power,1,na);
    setVal('me',d.energy,3,false);
    for(var i=0;i<N;i++){
      var le=document.getElementById('lim'+i);
      if(document.activeElement!==le)le.value=d.limits[i];
      document.getElementById('en'+i).checked=d.enabled[i];
    }
    updMeters(d);
    updBadges(d);
    updStats(d);
    updProtection(d);
    updBypassBtn(!!d.bypass);
  }catch(ex){}
}
async function saveLimits(){
  var q='';
  for(var i=0;i<N;i++){
    var v=parseFloat(document.getElementById('lim'+i).value)||5;
    q+=(q?'&':'?')+'l'+i+'='+v;
  }
  var r=await fetch('/api/setLimits'+q);
  var d=await r.json();
  toast(d.status==='ok'?'✓ Limits saved!':'Error saving limits',d.status==='ok'?'ok':'err');
}
async function setEn(i){
  var v=document.getElementById('en'+i).checked?1:0;
  await fetch('/api/setEnabled?idx='+i+'&val='+v);
  toast('Meter '+(i+1)+' '+(v?'enabled':'disabled'));
}
async function manSwitch(){
  var m=document.getElementById('manM').value;
  var r=await fetch('/api/switchMeter?m='+m);
  var d=await r.json();
  toast(d.status==='ok'?'Switched to Meter '+(parseInt(m)+1):'Switch failed',d.status==='ok'?'ok':'err');
}
async function resetEnergy(){
  var r=await fetch('/api/resetEnergy');
  var d=await r.json();
  toast(d.status==='ok'?'Energy counter reset':'Error','warn');
}
async function doEmergency(){
  var r=await fetch('/api/emergency');
  var d=await r.json();
  toast(d.status==='ok'?'Emergency OFF activated!':'Error','err');
}
// BUG FIX #2: bypass mode toggle — button reflects live state from poll()
var bypassOn=false;
function updBypassBtn(on){
  bypassOn=on;
  var b=document.getElementById('bypBtn');
  b.innerHTML='&#9193; Bypass: '+(on?'ON':'OFF');
}
async function toggleBypass(){
  var r=await fetch('/api/setBypass?val='+(bypassOn?0:1));
  var d=await r.json();
  if(d.status==='ok'){updBypassBtn(!bypassOn);toast('Bypass mode '+(bypassOn?'enabled':'disabled'),'warn');}
  else toast('Error setting bypass','err');
}
async function saveMeterCount(){
  var s=document.getElementById('mcount');
  var n=parseInt(s.value);
  var r=await fetch('/api/setActiveMeters?n='+n);
  var d=await r.json();
  if(d.status==='ok'){
    delete s.dataset.touched;
    toast('Active Meters set to '+n);
    poll();
  }else{
    toast('Error saving Active Meters','err');
  }
}
document.getElementById('mcount').addEventListener('change',function(){this.dataset.touched='1';});
poll();
setInterval(poll,3000);
</script>
</body>
</html>
)rawhtml";

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[BOOT] Smart Meter ATS System starting..."));

  // Mutex must exist before anything touches shared state
  stateMux = xSemaphoreCreateRecursiveMutex();

  setupPins();
  loadSettings();
  Serial.printf("[CFG] Active Meters: %d of %d supported\n",
    activeMeterCount, MAX_METERS);

  // BUG FIX #4: NVS may have stored an activeMeter that was later
  // disabled (or now sits outside the Active Meters window after a
  // count change). Never boot pointing at a disabled meter.
  if (activeMeter >= activeMeterCount || !meters[activeMeter].enabled) {
    int corrected = (activeMeter < activeMeterCount)
                      ? nextEnabledMeter(activeMeter)   // disabled → next enabled
                      : firstEnabledMeter();            // out of range → first enabled
    Serial.printf("[CFG] Saved active meter %d is disabled/out of range — corrected to %d\n",
      activeMeter + 1, corrected + 1);
    activeMeter = corrected;
  }

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  rtcOK = rtc.begin();          // probe DS3231 ONCE — reuse rtcOK everywhere
  if (!rtcOK) {
    Serial.println(F("[RTC] DS3231 not found! Check wiring."));
  } else {
    Serial.println(F("[RTC] DS3231 OK"));
    DateTime now = rtc.now();
    lastDay = now.day();
    Serial.printf("[RTC] Date: %04d-%02d-%02d %02d:%02d:%02d\n",
      now.year(), now.month(), now.day(),
      now.hour(), now.minute(), now.second());

    // BUG FIX #3: catch a monthly reset that was missed while the
    // device was powered off (e.g. off on the 1st, on again the 2nd).
    // Runs before pzemTask starts, so no lock is needed yet.
    if (lastResetMonth > 0 &&
        (now.month() != lastResetMonth || now.year() != lastResetYear)) {
      Serial.println(F("[RESET] Missed monthly reset detected at boot — applying now"));
      performMonthlyReset(now);
      saveSettings();
    } else if (lastResetMonth <= 0) {
      // First boot ever: adopt the current month as baseline
      lastResetMonth = now.month();
      lastResetYear  = now.year();
      saveSettings();
    }
  }

  // PZEM UART2 is initialised inside the PZEM004Tv30 constructor
  Serial.println(F("[PZEM] Hardware UART2 (GPIO16 RX / GPIO17 TX) ready"));

  setupWiFi();

  // Register web routes
  server.on("/",              HTTP_GET,  []() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  server.on("/api/status",          HTTP_GET, handleApiStatus);
  server.on("/api/setLimits",       HTTP_GET, handleApiSetLimits);
  server.on("/api/setEnabled",      HTTP_GET, handleApiSetEnabled);
  server.on("/api/switchMeter",     HTTP_GET, handleApiSwitchMeter);
  server.on("/api/resetEnergy",     HTTP_GET, handleApiResetEnergy);
  server.on("/api/emergency",       HTTP_GET, handleApiEmergency);
  server.on("/api/setProtection",   HTTP_GET, handleApiSetProtection);
  server.on("/api/clearFault",      HTTP_GET, handleApiClearFault);
  server.on("/api/setActiveMeters", HTTP_GET, handleApiSetActiveMeters);
  server.on("/api/setBypass",       HTTP_GET, handleApiSetBypass);  // BUG FIX #2
  server.begin();
  Serial.println(F("[WEB] HTTP server started"));

  // Task watchdog: arm on the Arduino loop task (this task, Core 1).
  // The pzemTask subscribes itself once it starts on Core 0.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // Core 3.x: reconfigure the already-initialised TWDT
  esp_task_wdt_config_t wdtCfg = {
    .timeout_ms    = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,        // don't watch idle tasks — we feed explicitly
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&wdtCfg);
#else
  // Core 2.x
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);       // subscribe current (loop) task
  Serial.printf("[WDT] Task watchdog armed (%ds, panic+reset)\n", WDT_TIMEOUT_S);

  // PZEM polling + protection on Core 0 (WiFi/web stays on Core 1)
  xTaskCreatePinnedToCore(
    pzemTask,        // task function
    "pzemTask",      // name
    4096,            // stack (bytes)
    NULL,            // param
    1,               // priority (low — same as loop task)
    &pzemTaskHandle,
    0);              // Core 0
  Serial.println(F("[TASK] PZEM/protection task started on Core 0"));

  // Activate first enabled meter
  // BUG FIX #8: emergencyOff is restored from NVS — if the system
  // was emergency-stopped before the reboot, it stays OFF.
  if (!emergencyOff) {
    switchToMeter(activeMeter);
  } else {
    Serial.println(F("[BOOT] Emergency state restored from NVS — all relays stay OFF"));
  }

  Serial.println(F("[BOOT] System ready."));
}

// ============================================================
//  MAIN LOOP  (Core 1)
// ============================================================
void loop() {
  esp_task_wdt_reset();       // feed the watchdog every pass
  server.handleClient();      // serve web requests
  handleButtons();            // read physical buttons
  handlePendingSwitch();      // finish a non-blocking meter switch
  handleRelayLogic();         // check limits, switch if needed
  handleTimeBasedResets();    // daily rollover & monthly reset
  handleDeferredSave();       // BUG FIX #7: throttled NVS flush
  printSerial();              // periodic debug output
}

// ============================================================
//  handleDeferredSave()  — BUG FIX #7: flash-wear protection.
//  Flushes dirty settings to NVS at most once every 30 seconds.
//  Critical paths (emergency, protection trip, monthly reset,
//  explicit user saves) still call saveSettings() directly.
// ============================================================
void handleDeferredSave() {
  if (!settingsDirty) return;
  if (millis() - lastNvsWrite < NVS_MIN_WRITE_INTERVAL_MS) return;
  settingsDirty = false;
  saveSettings();
}

// ============================================================
//  pzemTask()  (Core 0) — PZEM polling + protection checks
//  The PZEM Modbus transaction blocks ~100ms per register read;
//  isolating it here keeps the web server responsive on Core 1.
// ============================================================
void pzemTask(void* param) {
  esp_task_wdt_add(NULL);     // watch this task too
  for (;;) {
    readEnergyData();         // blocking Modbus reads — no mutex held
    handleProtection();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(PZEM_READ_INTERVAL));
  }
}

// ============================================================
//  setupPins()
//  ALL 10 relay pins are configured and driven OFF regardless
//  of Active Meters — unpopulated positions just stay OFF.
// ============================================================
void setupPins() {
  for (int i = 0; i < MAX_METERS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
  }
  pinMode(BTN_EMERGENCY, INPUT_PULLUP);
  Serial.printf("[PINS] %d relay outputs configured, all OFF\n", MAX_METERS);
}

// ============================================================
//  setupWiFi()  - creates standalone Access Point
// ============================================================
void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("[WiFi] AP started  SSID: %s  IP: %s\n",
    AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ============================================================
//  readEnergyData()  — runs on Core 0 (pzemTask)
//  Pacing comes from vTaskDelay() in pzemTask, not millis().
//  PZEM Modbus reads happen WITHOUT the mutex (they block ~100ms
//  each); only the shared-state update at the end is locked.
// ============================================================
void readEnergyData() {
  float v = pzem.voltage();
  float c = pzem.current();
  float p = pzem.power();
  float e = pzem.energy();   // kWh from PZEM

  bool anyValid = !isnan(v) || !isnan(c) || !isnan(p) || !isnan(e);

  STATE_LOCK();
  if (!isnan(v)) liveVoltage = v;
  if (!isnan(c)) liveCurrent = c;
  if (!isnan(p)) livePower   = p;
  if (!isnan(e)) {
    // Accumulate delta into daily / monthly statistics
    if (prevLiveEnergy >= 0.0f && e >= prevLiveEnergy) {
      float statDelta = e - prevLiveEnergy;
      if (statDelta < 100.0f) {        // sanity guard against PZEM counter wrap
        todayUsed        += statDelta;
        currentMonthUsed += statDelta;
      }
    } else if (prevLiveEnergy >= 0.0f && e < prevLiveEnergy) {
      // BUG FIX #5: PZEM counter went backwards — the module lost
      // power and reset its energy register to ~0. Log the event
      // and estimate the unrecorded energy from the average power
      // across the gap so daily/monthly stats and the active
      // meter's usage don't silently lose consumption.
      float gapHours = (millis() - lastEnergySampleMs) / 3600000.0f;
      if (gapHours < 0 || gapHours > 24.0f) gapHours = 0;   // millis wrap / absurd gap
      float estLost = (livePower / 1000.0f) * gapHours;     // kWh = kW * h
      Serial.printf("[PZEM] Energy counter RESET detected (%.3f -> %.3f kWh). "
                    "Estimated lost energy: %.3f kWh over %.2f h\n",
                    prevLiveEnergy, e, estLost, gapHours);
      todayUsed        += estLost;
      currentMonthUsed += estLost;
      if (!emergencyOff) {
        meters[activeMeter].energyUsed += estLost;
      }
      // Re-baseline per-meter tracking on the reset counter so the
      // active meter's usage keeps accumulating from here instead
      // of jumping to a huge (old-base) or negative delta.
      pzemEnergyBase = e - meters[activeMeter].energyUsed;
      settingsDirty  = true;
    }
    prevLiveEnergy = e;
    lastEnergySampleMs = millis();   // BUG FIX #5: track sample time
    // Per-meter tracking
    float delta = e - pzemEnergyBase;
    if (delta < 0) delta = 0;
    liveEnergy = e;
    if (!emergencyOff) {
      meters[activeMeter].energyUsed = delta;
    }
  }
  // If all readings are NaN, PZEM is offline — zero out live values
  if (!anyValid) {
    liveVoltage = 0.0f;
    liveCurrent = 0.0f;
    livePower   = 0.0f;
  }
  pzemOK = anyValid;
  STATE_UNLOCK();
}

// ============================================================
//  handleRelayLogic()
// ============================================================
void handleRelayLogic() {
  bool needSave = false;
  STATE_LOCK();
  // pendingMeter >= 0: a switch is already in flight — don't start another
  // BUG FIX #2: bypass mode disables automatic limit-based switching
  if (!emergencyOff && !protTrip && !bypassMode && pendingMeter < 0) {
    // Check if active meter has reached its limit
    if (meters[activeMeter].energyLimit > 0 &&
        meters[activeMeter].energyUsed >= meters[activeMeter].energyLimit) {

      Serial.printf("[SWITCH] Meter %d limit (%.2f kWh) reached. Switching...\n",
        activeMeter + 1, meters[activeMeter].energyLimit);

      int next = nextEnabledMeter(activeMeter);
      if (next == activeMeter) {
        // No other meter available — stay but stop (all limits hit)
        Serial.println(F("[SWITCH] All meters exhausted or disabled."));
      } else {
        // Capture current PZEM energy as base for next meter
        pzemEnergyBase = liveEnergy;
        switchToMeter(next);
        needSave = true;
      }
    }
  }
  STATE_UNLOCK();
  // BUG FIX #7: routine limit-hit switch only marks settings dirty;
  // handleDeferredSave() flushes to NVS at most once per 30s.
  if (needSave) settingsDirty = true;
}

// ============================================================
//  performMonthlyReset()  — shared by the scheduled rollover and
//  the boot-time missed-reset check (BUG FIX #3).
//  Caller must hold STATE_LOCK (or be single-threaded at boot).
// ============================================================
void performMonthlyReset(const DateTime& now) {
  Serial.println(F("[RESET] Monthly reset triggered!"));
  lastMonthUsed    = currentMonthUsed;
  currentMonthUsed = 0.0f;
  for (int i = 0; i < MAX_METERS; i++) meters[i].energyUsed = 0;
  pzemEnergyBase = liveEnergy;
  emergencyOff = false;
  switchToMeter(firstEnabledMeter());
  // BUG FIX #3: record when the reset happened so a reset missed
  // while powered off is detected and applied at next boot.
  lastResetMonth = now.month();
  lastResetYear  = now.year();
}

// ============================================================
//  handleTimeBasedResets()  — daily rollover & monthly reset
//  BUG FIX #3: the monthly reset no longer requires being awake
//  exactly on the 1st. It fires whenever the current (year,month)
//  differs from the (year,month) of the last recorded reset —
//  so powering on the 2nd (or the 15th) still triggers it.
//  BUG FIX #9: lastTrackedDay/lastTrackedMonth are now updated
//  unconditionally at the end of every check, not only inside the
//  day-changed branch, so the tracking state can never go stale.
// ============================================================
void handleTimeBasedResets() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 60000) return; // check every minute
  lastCheck = millis();

  if (!rtcOK) return;              // probed once in setup()
  DateTime now = rtc.now();

  // First-run: initialise tracking variables from live RTC
  if (lastTrackedDay == -1) {
    lastTrackedDay   = now.day();
    lastTrackedMonth = now.month();
    lastDay          = now.day();
    return;
  }

  bool needSave = false;
  STATE_LOCK();

  // Day changed → archive yesterday, reset today counter
  if (now.day() != lastTrackedDay) {
    dailyUsage[dailyIndex] = todayUsed;
    dailyIndex = (dailyIndex + 1) % 30;
    todayUsed  = 0.0f;
    needSave = true;
  }

  // BUG FIX #3: month boundary detection independent of the day.
  // (lastResetMonth <= 0 means "never reset" — adopt the current
  // month as the baseline without wiping counters.)
  if (lastResetMonth <= 0) {
    lastResetMonth = now.month();
    lastResetYear  = now.year();
    needSave = true;
  } else if (now.month() != lastResetMonth || now.year() != lastResetYear) {
    performMonthlyReset(now);
    needSave = true;
  }

  // BUG FIX #9: tracking variables always updated, every check
  lastTrackedDay   = now.day();
  lastTrackedMonth = now.month();
  lastDay          = now.day();
  STATE_UNLOCK();

  // Calendar rollovers are rare and define billing boundaries —
  // write NVS immediately rather than deferring (BUG FIX #7 exempts
  // critical/rare events from throttling).
  if (needSave) saveSettings();
}

// ============================================================
//  handleButtons()
// ============================================================
void handleButtons() {
  unsigned long now = millis();

  // --- Emergency OFF ---
  bool emgState = digitalRead(BTN_EMERGENCY);
  if (emgState == LOW && btnEmergLast == HIGH && (now - btnEmergTime) > BTN_DEBOUNCE_MS) {
    btnEmergTime = now;
    STATE_LOCK();
    emergencyOff = true;
    allRelaysOff();               // also cancels any pending switch
    STATE_UNLOCK();
    Serial.println(F("[BTN] EMERGENCY OFF!"));
    // BUG FIX #8: critical event — persist immediately (bypasses
    // the BUG FIX #7 throttle) so the state survives a reboot.
    saveSettings();
  }
  btnEmergLast = emgState;
}

// ============================================================
//  switchToMeter()  — non-blocking safe relay switching
//  Turns all relays OFF and commits activeMeter immediately;
//  handlePendingSwitch() turns the new relay ON after
//  RELAY_SWITCH_DELAY has elapsed (loop keeps running meanwhile).
//  Only meters below activeMeterCount are ever switched to.
//  Caller must hold STATE_LOCK (recursive, so re-locking is fine).
// ============================================================
void switchToMeter(int meterIndex) {
  STATE_LOCK();
  if (meterIndex < 0 || meterIndex >= activeMeterCount) {
    STATE_UNLOCK();
    return;
  }
  if (!meters[meterIndex].enabled) {
    meterIndex = nextEnabledMeter(meterIndex);
  }

  // Step 1: Turn ALL relays OFF first (also clears pendingMeter)
  allRelaysOff();

  // Step 2: schedule relay-ON after RELAY_SWITCH_DELAY (non-blocking)
  activeMeter   = meterIndex;   // committed now — save/tracking see new meter
  pendingMeter  = meterIndex;
  switchOffTime = millis();
  STATE_UNLOCK();
  Serial.printf("[RELAY] Switch to Meter %d scheduled (+%dms)\n",
    meterIndex + 1, RELAY_SWITCH_DELAY);
}

// ============================================================
//  handlePendingSwitch()  — completes a scheduled meter switch
//  BUG FIX #1: besides pendingMeter being cleared by
//  allRelaysOff(), re-verify protTrip / emergencyOff / bounds
//  under the mutex before energising. Both cores serialise on
//  stateMux, so after a protection trip (Core 0) has run
//  allRelaysOff(), no path can reach the digitalWrite(ON) here:
//  the pending switch is cancelled AND the guards below reject
//  any stale state. Relay ON is only ever possible when no
//  fault/emergency is latched.
// ============================================================
void handlePendingSwitch() {
  STATE_LOCK();
  if (pendingMeter >= 0 && (millis() - switchOffTime) >= RELAY_SWITCH_DELAY) {
    if (protTrip || emergencyOff || pendingMeter >= activeMeterCount) {
      // Fault/emergency latched while the switch was in flight —
      // never energise; drop the pending switch instead.
      Serial.println(F("[RELAY] Pending switch cancelled (fault/emergency)"));
      pendingMeter = -1;
    } else {
      digitalWrite(RELAY_PINS[pendingMeter], RELAY_ON);
      Serial.printf("[RELAY] Meter %d ON\n", pendingMeter + 1);
      pendingMeter = -1;
    }
  }
  STATE_UNLOCK();
}

// ============================================================
//  allRelaysOff()  — caller must hold STATE_LOCK
//  Drives ALL 10 relay positions OFF (populated or not) and
//  cancels any in-flight switch so a pending relay-ON can never
//  fire after an emergency stop or protection trip.
// ============================================================
void allRelaysOff() {
  pendingMeter = -1;
  for (int i = 0; i < MAX_METERS; i++) {
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
  }
  Serial.println(F("[RELAY] All relays OFF"));
}

// ============================================================
//  nextEnabledMeter()  — wraps within the Active Meters window
// ============================================================
int nextEnabledMeter(int current) {
  for (int i = 1; i <= activeMeterCount; i++) {
    int candidate = (current + i) % activeMeterCount;
    if (meters[candidate].enabled) {
      return candidate;
    }
  }
  return current; // no other meter enabled
}

// ============================================================
//  firstEnabledMeter()  — lowest enabled index in the window,
//  falls back to 0 if the user disabled everything
// ============================================================
int firstEnabledMeter() {
  for (int i = 0; i < activeMeterCount; i++) {
    if (meters[i].enabled) return i;
  }
  return 0;
}

// ============================================================
//  saveSettings()  — NVS via Preferences (named keys, no offsets)
//  Snapshots shared state under the mutex, then does the slow
//  NVS writes unlocked so Core 0 is never blocked by flash I/O.
//  Always persists all MAX_METERS slots so settings survive a
//  later increase of Active Meters.
// ============================================================
void saveSettings() {
  float   limSnap[MAX_METERS];
  bool    enSnap[MAX_METERS];
  float   dailySnap[30];
  uint8_t countSnap, activeSnap, didxSnap;
  float   todaySnap, curmonSnap, lastmonSnap, ovSnap, uvSnap, ocSnap;
  bool    emergSnap, bypassSnap;
  uint8_t rstmonSnap;
  uint16_t rstyrSnap;

  STATE_LOCK();
  for (int i = 0; i < MAX_METERS; i++) {
    limSnap[i] = meters[i].energyLimit;
    enSnap[i]  = meters[i].enabled;
  }
  countSnap   = activeMeterCount;
  activeSnap  = (uint8_t)activeMeter;
  memcpy(dailySnap, dailyUsage, sizeof(dailySnap));
  didxSnap    = (uint8_t)constrain(dailyIndex, 0, 29);
  todaySnap   = todayUsed;
  curmonSnap  = currentMonthUsed;
  lastmonSnap = lastMonthUsed;
  ovSnap      = ovVoltThresh;
  uvSnap      = uvVoltThresh;
  ocSnap      = ocCurrThresh;
  emergSnap   = emergencyOff;                              // BUG FIX #8
  bypassSnap  = bypassMode;                                // BUG FIX #2
  rstmonSnap  = (uint8_t)max(lastResetMonth, 0);           // BUG FIX #3
  rstyrSnap   = (uint16_t)max(lastResetYear, 0);           // BUG FIX #3
  // BUG FIX #7: clear the dirty flag while still holding the lock —
  // anything dirtied AFTER this snapshot re-raises the flag and gets
  // picked up by the next deferred flush instead of being lost.
  settingsDirty = false;
  STATE_UNLOCK();

  prefs.begin(NVS_NAMESPACE, false);   // read-write
  char key[8];
  prefs.putUChar("mcount", countSnap);
  for (int i = 0; i < MAX_METERS; i++) {
    snprintf(key, sizeof(key), "lim%d", i);
    prefs.putFloat(key, limSnap[i]);
    snprintf(key, sizeof(key), "en%d", i);
    prefs.putBool(key, enSnap[i]);
  }
  prefs.putUChar("active", activeSnap);
  // Daily statistics — store the whole ring buffer as one blob
  prefs.putBytes("daily", dailySnap, sizeof(dailySnap));
  prefs.putUChar("didx", didxSnap);
  prefs.putFloat("today",   todaySnap);
  prefs.putFloat("curmon",  curmonSnap);
  prefs.putFloat("lastmon", lastmonSnap);
  prefs.putFloat("ov", ovSnap);
  prefs.putFloat("uv", uvSnap);
  prefs.putFloat("oc", ocSnap);
  prefs.putBool("emerg",  emergSnap);    // BUG FIX #8
  prefs.putBool("bypass", bypassSnap);   // BUG FIX #2
  prefs.putUChar("rstmon", rstmonSnap);  // BUG FIX #3
  prefs.putUShort("rstyr", rstyrSnap);   // BUG FIX #3
  prefs.end();
  lastNvsWrite = millis();               // BUG FIX #7: throttle baseline
  Serial.println(F("[NVS] Settings saved"));
}

// ============================================================
//  loadSettings()  — NVS via Preferences (defaults if key absent)
//  Runs once in setup() before pzemTask starts — no lock needed.
// ============================================================
void loadSettings() {
  // Default values for ALL slots
  for (int i = 0; i < MAX_METERS; i++) {
    meters[i].energyLimit = 5.0f;
    meters[i].enabled     = true;
    meters[i].energyUsed  = 0.0f;
  }
  activeMeterCount = DEFAULT_ACTIVE_METERS;
  activeMeter = 0;

  if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only; fails if namespace never written
    Serial.println(F("[NVS] No saved data - using defaults"));
    return;
  }

  activeMeterCount = constrain(prefs.getUChar("mcount", DEFAULT_ACTIVE_METERS),
                               1, MAX_METERS);

  char key[8];
  for (int i = 0; i < MAX_METERS; i++) {
    snprintf(key, sizeof(key), "lim%d", i);
    meters[i].energyLimit = prefs.getFloat(key, 5.0f);
    if (isnan(meters[i].energyLimit) || meters[i].energyLimit <= 0) {
      meters[i].energyLimit = 5.0f;
    }
    snprintf(key, sizeof(key), "en%d", i);
    meters[i].enabled = prefs.getBool(key, true);
  }
  activeMeter = constrain(prefs.getUChar("active", 0), 0, activeMeterCount - 1);
  // Daily statistics
  size_t got = prefs.getBytes("daily", dailyUsage, sizeof(dailyUsage));
  if (got != sizeof(dailyUsage)) {
    for (int i = 0; i < 30; i++) dailyUsage[i] = 0.0f;
  } else {
    for (int i = 0; i < 30; i++) {
      if (isnan(dailyUsage[i]) || dailyUsage[i] < 0) dailyUsage[i] = 0.0f;
    }
  }
  dailyIndex = (int)constrain(prefs.getUChar("didx", 0), 0, 29);
  todayUsed        = prefs.getFloat("today",   0.0f);
  currentMonthUsed = prefs.getFloat("curmon",  0.0f);
  lastMonthUsed    = prefs.getFloat("lastmon", 0.0f);
  if (isnan(todayUsed)        || todayUsed < 0)        todayUsed = 0.0f;
  if (isnan(currentMonthUsed) || currentMonthUsed < 0) currentMonthUsed = 0.0f;
  if (isnan(lastMonthUsed)    || lastMonthUsed < 0)    lastMonthUsed = 0.0f;
  // Protection thresholds
  ovVoltThresh = prefs.getFloat("ov", 250.0f);
  uvVoltThresh = prefs.getFloat("uv", 180.0f);
  ocCurrThresh = prefs.getFloat("oc", 16.0f);
  if (isnan(ovVoltThresh) || ovVoltThresh < 220 || ovVoltThresh > 300) ovVoltThresh = 250.0f;
  if (isnan(uvVoltThresh) || uvVoltThresh < 100 || uvVoltThresh > 220) uvVoltThresh = 180.0f;
  if (isnan(ocCurrThresh) || ocCurrThresh < 0.1 || ocCurrThresh > 200) ocCurrThresh = 16.0f;
  emergencyOff   = prefs.getBool("emerg",  false);   // BUG FIX #8
  bypassMode     = prefs.getBool("bypass", false);   // BUG FIX #2
  lastResetMonth = (int)prefs.getUChar("rstmon", 0); // BUG FIX #3
  lastResetYear  = (int)prefs.getUShort("rstyr", 0); // BUG FIX #3
  if (lastResetMonth < 1 || lastResetMonth > 12) lastResetMonth = -1;
  if (lastResetYear  < 2020) { lastResetMonth = -1; lastResetYear = -1; }
  prefs.end();

  Serial.println(F("[NVS] Settings loaded"));
}

// ============================================================
//  printSerial()  - periodic debug output
// ============================================================
void printSerial() {
  if (millis() - lastSerialPrint < SERIAL_PRINT_INTERVAL) return;
  lastSerialPrint = millis();

  STATE_LOCK();
  float  v = liveVoltage, c = liveCurrent, p = livePower, e = liveEnergy;
  int    am = activeMeter, count = activeMeterCount;
  bool   emg = emergencyOff, trip = protTrip, byp = bypassMode;
  String reason = protReason;
  STATE_UNLOCK();

  Serial.println(F("------ STATUS ------"));
  Serial.printf("Voltage  : %.1f V\n",  v);
  Serial.printf("Current  : %.2f A\n",  c);
  Serial.printf("Power    : %.1f W\n",  p);
  Serial.printf("Energy   : %.3f kWh\n",e);
  Serial.printf("Meters   : %d active\n", count);
  Serial.printf("Active   : Meter %d\n",am + 1);
  for (int i = 0; i < count; i++) {
    Serial.printf("Relay%-2d  : %s\n", i + 1,
      digitalRead(RELAY_PINS[i]) == RELAY_ON ? "ON" : "OFF");
  }
  Serial.printf("Mode     : %s\n", emg ? "EMERGENCY" : byp ? "BYPASS" : "AUTO");
  if (trip) Serial.printf("PROT     : TRIP - %s\n", reason.c_str());
  Serial.println(F("--------------------"));
}

// ============================================================
//  WEB API HANDLERS
//  All endpoints work on meter indices 0..activeMeterCount-1.
//  GPIO numbers never appear in any request or response.
// ============================================================

// GET /api/status  -> JSON (arrays sized to Active Meters only)
void handleApiStatus() {
  StaticJsonDocument<3072> doc;

  STATE_LOCK();
  doc["voltage"]     = pzemOK ? liveVoltage : 0.0f;
  doc["current"]     = pzemOK ? liveCurrent : 0.0f;
  doc["power"]       = pzemOK ? livePower   : 0.0f;
  doc["energy"]      = liveEnergy;
  doc["meterCount"]  = activeMeterCount;
  doc["maxMeters"]   = MAX_METERS;
  doc["activeMeter"] = activeMeter;
  doc["emergency"]   = emergencyOff;
  doc["bypass"]      = bypassMode;   // BUG FIX #2: dashboard reads d.bypass
  doc["pzemOK"]      = pzemOK;
  doc["todayUsed"]   = todayUsed;
  doc["thisMonth"]   = currentMonthUsed;
  doc["lastMonth"]   = lastMonthUsed;
  doc["protTrip"]    = protTrip;
  doc["protReason"]  = protReason;
  doc["ovVolt"]      = ovVoltThresh;
  doc["uvVolt"]      = uvVoltThresh;
  doc["ocCurr"]      = ocCurrThresh;

  JsonArray lims  = doc.createNestedArray("limits");
  JsonArray enab  = doc.createNestedArray("enabled");
  JsonArray used  = doc.createNestedArray("used");
  JsonArray daily = doc.createNestedArray("daily");
  // Only the active window is serialised — hidden meters are ignored
  for (int i = 0; i < activeMeterCount; i++) {
    lims.add(meters[i].energyLimit);
    enab.add(meters[i].enabled);
    used.add(meters[i].energyUsed);
  }
  // daily[]: oldest entry first, current day last
  for (int i = 0; i < 30; i++) {
    daily.add(dailyUsage[(dailyIndex + i) % 30]);
  }
  STATE_UNLOCK();

  // RTC time so dashboard can compute next reset date
  // (probed once in setup — no repeated rtc.begin() I2C churn)
  if (rtcOK) {
    DateTime now = rtc.now();
    doc["rtcDay"]   = now.day();
    doc["rtcMonth"] = now.month();
    doc["rtcYear"]  = now.year();
  }

  String out;
  serializeJson(doc, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", out);
}

// GET /api/setActiveMeters?n=1..10
// The ONE per-installation setting. Persists in NVS; the
// dashboard rebuilds itself from the new count on next poll.
void handleApiSetActiveMeters() {
  bool ok = false;
  if (server.hasArg("n")) {
    int n = server.arg("n").toInt();
    if (n >= 1 && n <= MAX_METERS) {
      STATE_LOCK();
      activeMeterCount = (uint8_t)n;
      // If the active meter fell outside the new window,
      // move to the first enabled meter inside it.
      if (activeMeter >= activeMeterCount) {
        allRelaysOff();
        if (!emergencyOff && !protTrip) {
          pzemEnergyBase = liveEnergy;
          switchToMeter(firstEnabledMeter());
        } else {
          activeMeter = firstEnabledMeter();
        }
      }
      STATE_UNLOCK();
      saveSettings();
      Serial.printf("[CFG] Active Meters set to %d\n", n);
      ok = true;
    }
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (ok) server.send(200, "application/json", "{\"status\":\"ok\"}");
  else    server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"n must be 1-10\"}");
}

// GET /api/setBypass?val=0/1
// BUG FIX #2: bypass mode — automatic limit-based switching is
// suspended; manual switching (and limit checks in it) relaxed.
void handleApiSetBypass() {
  bool ok = false;
  if (server.hasArg("val")) {
    bool v = (server.arg("val").toInt() == 1);
    STATE_LOCK();
    bypassMode = v;
    STATE_UNLOCK();
    saveSettings();
    Serial.printf("[CFG] Bypass mode %s\n", v ? "ENABLED" : "DISABLED");
    ok = true;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (ok) server.send(200, "application/json", "{\"status\":\"ok\"}");
  else    server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"val required\"}");
}

// GET /api/setLimits?l0=5.0&l1=5.0&...&l9=5.0 (only active window used)
void handleApiSetLimits() {
  char argName[8];
  STATE_LOCK();
  for (int i = 0; i < activeMeterCount; i++) {
    snprintf(argName, sizeof(argName), "l%d", i);
    if (server.hasArg(argName)) {
      meters[i].energyLimit = server.arg(argName).toFloat();
    }
    // Clamp to sane range
    if (meters[i].energyLimit <= 0 || meters[i].energyLimit > 9999) {
      meters[i].energyLimit = 5.0f;
    }
  }
  // BUG FIX #10: if the active meter's NEW limit is already exceeded,
  // switch immediately instead of waiting for the next relay-logic
  // cycle. Respects the same guards as automatic switching.
  if (!emergencyOff && !protTrip && !bypassMode && pendingMeter < 0 &&
      meters[activeMeter].energyLimit > 0 &&
      meters[activeMeter].energyUsed >= meters[activeMeter].energyLimit) {
    int next = nextEnabledMeter(activeMeter);
    if (next != activeMeter) {
      Serial.printf("[SWITCH] New limit for Meter %d already exceeded — switching now\n",
        activeMeter + 1);
      pzemEnergyBase = liveEnergy;
      switchToMeter(next);
    }
  }
  STATE_UNLOCK();
  saveSettings();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/setEnabled?idx=0..N-1&val=0/1
void handleApiSetEnabled() {
  if (server.hasArg("idx") && server.hasArg("val")) {
    int idx = server.arg("idx").toInt();
    STATE_LOCK();
    bool valid = (idx >= 0 && idx < activeMeterCount);
    if (valid) {
      meters[idx].enabled = (server.arg("val").toInt() == 1);
    }
    STATE_UNLOCK();
    if (valid) saveSettings();
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/switchMeter?m=0..N-1
void handleApiSwitchMeter() {
  bool ok = false;
  String err = "invalid meter";
  if (server.hasArg("m")) {
    int m = server.arg("m").toInt();
    bool doSave = false;
    STATE_LOCK();
    if (m >= 0 && m < activeMeterCount && meters[m].enabled && !protTrip) {
      // BUG FIX #6: reject switching to a meter that already reached
      // its energy limit — unless bypass mode is active.
      if (!bypassMode && meters[m].energyLimit > 0 &&
          meters[m].energyUsed >= meters[m].energyLimit) {
        err = "meter limit reached";
      } else {
        emergencyOff = false;
        pzemEnergyBase = liveEnergy;
        switchToMeter(m);
        doSave = true;
        ok = true;
      }
    } else if (protTrip) {
      err = "protection trip active";
    }
    STATE_UNLOCK();
    if (doSave) saveSettings();
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (ok) server.send(200, "application/json", "{\"status\":\"ok\"}");
  else    server.send(409, "application/json",
                      String("{\"status\":\"error\",\"msg\":\"") + err + "\"}");
}

// GET /api/resetEnergy
void handleApiResetEnergy() {
  STATE_LOCK();
  for (int i = 0; i < MAX_METERS; i++) {
    meters[i].energyUsed = 0;
  }
  pzemEnergyBase = liveEnergy; // new baseline
  STATE_UNLOCK();
  Serial.println(F("[WEB] Energy reset via dashboard"));
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/emergency
void handleApiEmergency() {
  STATE_LOCK();
  emergencyOff = true;
  allRelaysOff();
  STATE_UNLOCK();
  Serial.println(F("[WEB] Emergency OFF via dashboard"));
  // BUG FIX #8: critical event — persist immediately
  saveSettings();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/setProtection?ov=250&uv=180&oc=16
void handleApiSetProtection() {
  STATE_LOCK();
  if (server.hasArg("ov")) {
    float v = server.arg("ov").toFloat();
    if (v >= 220.0f && v <= 300.0f) ovVoltThresh = v;
  }
  if (server.hasArg("uv")) {
    float v = server.arg("uv").toFloat();
    if (v >= 100.0f && v <= 220.0f) uvVoltThresh = v;
  }
  if (server.hasArg("oc")) {
    float v = server.arg("oc").toFloat();
    if (v >= 0.1f && v <= 200.0f) ocCurrThresh = v;
  }
  STATE_UNLOCK();
  saveSettings();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/clearFault
void handleApiClearFault() {
  STATE_LOCK();
  if (protTrip) {
    protTrip   = false;
    protReason = "";
    Serial.println(F("[PROT] Fault cleared via dashboard"));
    if (!emergencyOff) {
      switchToMeter(activeMeter);
    }
  }
  STATE_UNLOCK();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ============================================================
//  handleProtection()  — over/under voltage & over current
//  Runs on Core 0 (pzemTask), right after readEnergyData().
//  One PZEM monitors the common line, so protection is global:
//  a trip cuts ALL relay positions regardless of Active Meters.
// ============================================================
void handleProtection() {
  STATE_LOCK();
  if (!pzemOK || protTrip) {
    STATE_UNLOCK();
    return;
  }

  bool tripped = false;
  if (liveVoltage > ovVoltThresh) {
    protReason = "Over Voltage: " + String(liveVoltage, 1) + "V (limit " + String(ovVoltThresh, 0) + "V)";
    tripped = true;
  } else if (liveVoltage > 10.0f && liveVoltage < uvVoltThresh) {
    protReason = "Under Voltage: " + String(liveVoltage, 1) + "V (limit " + String(uvVoltThresh, 0) + "V)";
    tripped = true;
  } else if (liveCurrent > ocCurrThresh) {
    protReason = "Over Current: " + String(liveCurrent, 2) + "A (limit " + String(ocCurrThresh, 1) + "A)";
    tripped = true;
  }

  if (tripped) {
    protTrip = true;
    allRelaysOff();   // also cancels any pending switch on Core 1
    Serial.printf("[PROT] TRIP: %s\n", protReason.c_str());
  }
  STATE_UNLOCK();
  // BUG FIX #7: protection trip is a critical event — persist
  // immediately. The NVS write happens AFTER releasing the mutex
  // (never hold stateMux across flash I/O; saveSettings() takes
  // its own short lock for the snapshot).
  if (tripped) saveSettings();
}
