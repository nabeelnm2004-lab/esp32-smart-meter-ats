/*
 * ============================================================
 *  Smart Automatic Energy Meter Switching System
 *  Board  : ESP8266 NodeMCU (ESP-12E)
 *  Author : Generated from project prompt
 *  Date   : 2026
 *
 *  WIRING SUMMARY
 *  ---------------
 *  Relay1        -> D5 (GPIO14)
 *  Relay2        -> D6 (GPIO12)
 *  Relay3        -> D7 (GPIO13)
 *  PZEM RX(ESP)  -> D3 (GPIO0)    (PZEM TX -> ESP D3)
 *  PZEM TX(ESP)  -> D4 (GPIO2)    (ESP D4 -> PZEM RX)
 *  RTC SDA       -> D2 (GPIO4)
 *  RTC SCL       -> D1 (GPIO5)
 *  Btn EmergOFF  -> D8 (GPIO15)
 *
 *  WiFi Access Point
 *  SSID: SmartMeterATS   Password: 12345678
 *  Dashboard: http://192.168.4.1/
 *
 *  REQUIRED LIBRARIES (install via Arduino Library Manager)
 *  - PZEM004Tv30  by Jakub Mandula
 *  - RTClib       by Adafruit
 *  - ArduinoJson  by Benoit Blanchon
 *  (ESP8266WiFi, ESP8266WebServer, EEPROM bundled with ESP8266 core)
 * ============================================================
 */

// ============================================================
//  INCLUDES
// ============================================================
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

// ============================================================
//  PIN DEFINITIONS
// ============================================================
#define RELAY1_PIN      14  // D5
#define RELAY2_PIN      12  // D6
#define RELAY3_PIN      13  // D7
#define PZEM_RX_PIN     0   // D3  (receives from PZEM TX)
#define PZEM_TX_PIN     2   // D4  (sends to PZEM RX)
#define BTN_EMERGENCY   15  // D8

// Relay logic: most relay modules are ACTIVE LOW
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// Number of meters
#define NUM_METERS  3

// ============================================================
//  WIFI ACCESS POINT
// ============================================================
const char* AP_SSID     = "SmartMeterATS";
const char* AP_PASSWORD = "12345678";
IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

// ============================================================
//  EEPROM LAYOUT
// ============================================================
#define EEPROM_SIZE          256
#define EEPROM_MAGIC_ADDR    0
#define EEPROM_MAGIC_VALUE   0xAB
#define EEPROM_LIMITS_ADDR   1   // 3 floats x 4 bytes = 12 bytes
#define EEPROM_ENABLED_ADDR  13  // 3 bytes
#define EEPROM_ACTIVE_ADDR   16  // 1 byte
#define EEPROM_BYPASS_ADDR   17  // 1 byte
#define EEPROM_DAILY_ADDR    18  // 30 floats x 4 = 120 bytes  (addr 18..137)
#define EEPROM_DIDX_ADDR     138 // 1 byte  — rolling daily write index
#define EEPROM_TODAY_ADDR    139 // float   — today kWh
#define EEPROM_CURMON_ADDR   143 // float   — current month kWh
#define EEPROM_LASTMON_ADDR  147 // float   — last month kWh
#define EEPROM_OV_ADDR       151 // float   — over-voltage threshold (V)
#define EEPROM_UV_ADDR       155 // float   — under-voltage threshold (V)
#define EEPROM_OC_ADDR       159 // float   — over-current threshold (A)

// ============================================================
//  TIMING CONSTANTS (ms)
// ============================================================
#define RELAY_SWITCH_DELAY   500
#define PZEM_READ_INTERVAL   2000
#define SERIAL_PRINT_INTERVAL 3000
#define BTN_DEBOUNCE_MS      50

// ============================================================
//  OBJECTS
// ============================================================
SoftwareSerial       pzemSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30          pzem(pzemSerial);
RTC_DS3231           rtc;
ESP8266WebServer     server(80);

// ============================================================
//  SYSTEM STATE
// ============================================================
struct MeterConfig {
  float energyLimit;   // kWh limit set by user
  bool  enabled;       // is this meter enabled?
  float energyUsed;    // accumulated kWh for this meter (session)
};

MeterConfig meters[NUM_METERS];

int  activeMeter     = 0;     // 0-indexed active meter (0,1,2)
bool bypassMode      = false; // manual bypass active
bool emergencyOff    = false; // all relays OFF
int  lastDay         = -1;    // for monthly reset detection

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

// Protection thresholds & fault state
float ovVoltThresh   = 250.0f;  // V  over-voltage limit
float uvVoltThresh   = 180.0f;  // V  under-voltage limit
float ocCurrThresh   = 16.0f;   // A  over-current limit
bool  protTrip       = false;   // latched protection trip
String protReason    = "";      // human-readable trip reason

// Timestamps (non-blocking)
unsigned long lastPzemRead    = 0;
unsigned long lastSerialPrint = 0;

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
void handleWebServer();
void saveSettings();
void loadSettings();
void switchToMeter(int meterIndex);
void allRelaysOff();
int  nextEnabledMeter(int current);
void printSerial();
void handleProtection();
void handleApiSetProtection();
void handleApiClearFault();

// ============================================================
//  EMBEDDED HTML DASHBOARD
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
      <p style=\"color:#3b82f6;font-weight:700;letter-spacing:.05em\">Eng-Nabeel</p>
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
      <div class="meter-list">
        <div class="mi" id="mi0"><div class="mi-top"><div class="mi-name"><span id="ad0"></span>Meter 1</div><span class="mt act" id="mt0">STANDBY</span></div><div class="bwrap"><div class="bfill" id="bar0" style="width:0%"></div></div><div class="mstats"><span id="us0">0.000 kWh</span><span id="lm0">/ 5.0 kWh</span></div></div>
        <div class="mi" id="mi1"><div class="mi-top"><div class="mi-name"><span id="ad1"></span>Meter 2</div><span class="mt act" id="mt1">STANDBY</span></div><div class="bwrap"><div class="bfill" id="bar1" style="width:0%"></div></div><div class="mstats"><span id="us1">0.000 kWh</span><span id="lm1">/ 5.0 kWh</span></div></div>
        <div class="mi" id="mi2"><div class="mi-top"><div class="mi-name"><span id="ad2"></span>Meter 3</div><span class="mt act" id="mt2">STANDBY</span></div><div class="bwrap"><div class="bfill" id="bar2" style="width:0%"></div></div><div class="mstats"><span id="us2">0.000 kWh</span><span id="lm2">/ 5.0 kWh</span></div></div>
      </div>
    </div>
    <div class="card">
      <div class="ctitle">&#9881; Energy Limits</div>
      <div class="lrow"><div class="lhdr"><span class="llbl">Meter 1 Limit (kWh)</span><label class="toggle"><input type="checkbox" id="en0" onchange="setEn(0)"><span class="slider"></span></label></div><input class="linp" type="number" id="lim0" min="0.1" step="0.1" placeholder="e.g. 5.0"></div>
      <div class="lrow"><div class="lhdr"><span class="llbl">Meter 2 Limit (kWh)</span><label class="toggle"><input type="checkbox" id="en1" onchange="setEn(1)"><span class="slider"></span></label></div><input class="linp" type="number" id="lim1" min="0.1" step="0.1" placeholder="e.g. 5.0"></div>
      <div class="lrow"><div class="lhdr"><span class="llbl">Meter 3 Limit (kWh)</span><label class="toggle"><input type="checkbox" id="en2" onchange="setEn(2)"><span class="slider"></span></label></div><input class="linp" type="number" id="lim2" min="0.1" step="0.1" placeholder="e.g. 5.0"></div>
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
        <div class="cg"><div class="cg-lbl">Switch to Meter</div><select class="csel" id="manM"><option value="0">&#128268; Meter 1</option><option value="1">&#128268; Meter 2</option><option value="2">&#128268; Meter 3</option></select><button class="btn btn-green" onclick="manSwitch()">&#8594; Switch Meter</button></div>
        <div class="cg"><div class="cg-lbl">Energy Counter</div><button class="btn btn-amber" style="margin-top:23px" onclick="resetEnergy()">&#128259; Reset Energy</button></div>
        <div class="cg"><div class="cg-lbl">Emergency</div><button class="btn btn-red" style="margin-top:23px" onclick="doEmergency()">&#9888; Emergency OFF</button></div>
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
var _tt;
function toast(m,t){var e=document.getElementById('toast');e.textContent=m;e.className='show '+(t||'ok');clearTimeout(_tt);_tt=setTimeout(function(){e.className=''},3200);}
function setVal(id,v,d,na){var e=document.getElementById(id);if(na){e.textContent='--';e.className='mc-val na';}else{e.textContent=v.toFixed(d);e.className='mc-val';}}
function updMeters(d){
  for(var i=0;i<3;i++){
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
    var na=!d.pzemOK;
    setVal('mv',d.voltage,1,na);
    setVal('ma',d.current,2,na);
    setVal('mw',d.power,1,na);
    setVal('me',d.energy,3,false);
    for(var i=0;i<3;i++){document.getElementById('lim'+i).value=d.limits[i];document.getElementById('en'+i).checked=d.enabled[i];}
    updMeters(d);
    updBadges(d);
    updStats(d);
    updProtection(d);
  }catch(ex){}
}
async function saveLimits(){
  var v=[];
  for(var i=0;i<3;i++)v.push(parseFloat(document.getElementById('lim'+i).value)||5);
  var r=await fetch('/api/setLimits?l0='+v[0]+'&l1='+v[1]+'&l2='+v[2]);
  var d=await r.json();
  toast(d.status==='ok'?'&#10003; Limits saved!':'Error saving limits',d.status==='ok'?'ok':'err');
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

  setupPins();
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println(F("[RTC] DS3231 not found! Check wiring."));
  } else {
    Serial.println(F("[RTC] DS3231 OK"));
    DateTime now = rtc.now();
    lastDay = now.day();
    Serial.printf("[RTC] Date: %04d-%02d-%02d %02d:%02d:%02d\n",
      now.year(), now.month(), now.day(),
      now.hour(), now.minute(), now.second());
  }

  pzemSerial.begin(9600);
  Serial.println(F("[PZEM] Software serial started"));

  setupWiFi();

  // Register web routes
  server.on("/",              HTTP_GET,  []() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
  });
  server.on("/api/status",     HTTP_GET,  handleApiStatus);
  server.on("/api/setLimits",  HTTP_GET,  handleApiSetLimits);
  server.on("/api/setEnabled", HTTP_GET,  handleApiSetEnabled);
  server.on("/api/switchMeter",HTTP_GET,  handleApiSwitchMeter);
  server.on("/api/resetEnergy",HTTP_GET,  handleApiResetEnergy);
  server.on("/api/emergency",     HTTP_GET, handleApiEmergency);
  server.on("/api/setProtection", HTTP_GET, handleApiSetProtection);
  server.on("/api/clearFault",    HTTP_GET, handleApiClearFault);
  server.begin();
  Serial.println(F("[WEB] HTTP server started"));

  // Activate first enabled meter
  if (!emergencyOff) {
    switchToMeter(activeMeter);
  }

  Serial.println(F("[BOOT] System ready."));
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {
  server.handleClient();      // serve web requests
  handleButtons();            // read physical buttons
  readEnergyData();           // non-blocking PZEM read
  handleRelayLogic();         // check limits, switch if needed
  handleTimeBasedResets();    // daily rollover & monthly reset
  handleProtection();         // over/under voltage & over current
  printSerial();              // periodic debug output
}

// ============================================================
//  setupPins()
// ============================================================
void setupPins() {
  pinMode(RELAY1_PIN,    OUTPUT); digitalWrite(RELAY1_PIN,  RELAY_OFF);
  pinMode(RELAY2_PIN,    OUTPUT); digitalWrite(RELAY2_PIN,  RELAY_OFF);
  pinMode(RELAY3_PIN,    OUTPUT); digitalWrite(RELAY3_PIN,  RELAY_OFF);
  pinMode(BTN_EMERGENCY, INPUT_PULLUP);
  Serial.println(F("[PINS] Configured"));
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
//  readEnergyData()  - non-blocking PZEM read
// ============================================================
void readEnergyData() {
  if (millis() - lastPzemRead < PZEM_READ_INTERVAL) return;
  lastPzemRead = millis();

  float v = pzem.voltage();
  float c = pzem.current();
  float p = pzem.power();
  float e = pzem.energy();   // kWh from PZEM

  // Validate readings — NaN means PZEM not connected/responding
  bool anyValid = false;
  if (!isnan(v)) { liveVoltage = v; anyValid = true; }
  if (!isnan(c)) { liveCurrent = c; anyValid = true; }
  if (!isnan(p)) { livePower   = p; anyValid = true; }
  if (!isnan(e)) {
    anyValid = true;
    // Accumulate delta into daily / monthly statistics
    if (prevLiveEnergy >= 0.0f && e >= prevLiveEnergy) {
      float statDelta = e - prevLiveEnergy;
      if (statDelta < 100.0f) {        // sanity guard against PZEM counter wrap
        todayUsed        += statDelta;
        currentMonthUsed += statDelta;
      }
    }
    prevLiveEnergy = e;
    // Per-meter tracking
    float delta = e - pzemEnergyBase;
    if (delta < 0) delta = 0;
    liveEnergy = e;
    if (!emergencyOff && !bypassMode) {
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
}

// ============================================================
//  handleRelayLogic()
// ============================================================
void handleRelayLogic() {
  if (emergencyOff || bypassMode || protTrip) return;

  // Check if active meter has reached its limit
  if (meters[activeMeter].energyLimit > 0 &&
      meters[activeMeter].energyUsed >= meters[activeMeter].energyLimit) {

    Serial.printf("[SWITCH] Meter %d limit (%.2f kWh) reached. Switching...\n",
      activeMeter + 1, meters[activeMeter].energyLimit);

    int next = nextEnabledMeter(activeMeter);
    if (next == activeMeter) {
      // No other meter available — stay but stop (all limits hit)
      Serial.println(F("[SWITCH] All meters exhausted or disabled."));
      return;
    }

    // Capture current PZEM energy as base for next meter
    pzemEnergyBase = liveEnergy;

    switchToMeter(next);
    saveSettings();
  }
}

// ============================================================
//  handleTimeBasedResets()  — daily rollover & monthly reset
// ============================================================
void handleTimeBasedResets() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 60000) return; // check every minute
  lastCheck = millis();

  if (!rtc.begin()) return;
  DateTime now = rtc.now();

  // First-run: initialise tracking variables from live RTC
  if (lastTrackedDay == -1) {
    lastTrackedDay   = now.day();
    lastTrackedMonth = now.month();
    lastDay          = now.day();
    return;
  }

  // Day changed → archive yesterday, reset today counter
  if (now.day() != lastTrackedDay) {
    dailyUsage[dailyIndex] = todayUsed;
    dailyIndex = (dailyIndex + 1) % 30;
    todayUsed  = 0.0f;
    // Monthly rollover: triggered on 1st of a new month
    if (now.day() == 1 && now.month() != lastTrackedMonth) {
      Serial.println(F("[RESET] Monthly reset triggered!"));
      lastMonthUsed    = currentMonthUsed;
      currentMonthUsed = 0.0f;
      for (int i = 0; i < NUM_METERS; i++) meters[i].energyUsed = 0;
      pzemEnergyBase = liveEnergy;
      emergencyOff = false;
      bypassMode   = false;
      switchToMeter(0);
    }
    lastTrackedMonth = now.month();
    saveSettings();
  }

  lastTrackedDay = now.day();
  lastDay        = now.day();
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
    emergencyOff = true;
    bypassMode   = false;
    allRelaysOff();
    Serial.println(F("[BTN] EMERGENCY OFF!"));
  }
  btnEmergLast = emgState;
}

// ============================================================
//  switchToMeter()  - safe relay switching
// ============================================================
void switchToMeter(int meterIndex) {
  if (meterIndex < 0 || meterIndex >= NUM_METERS) return;
  if (!meters[meterIndex].enabled) {
    meterIndex = nextEnabledMeter(meterIndex);
  }

  // Step 1: Turn ALL relays OFF first
  allRelaysOff();

  // Step 2: Wait 500 ms for safe switching
  delay(RELAY_SWITCH_DELAY);

  // Step 3: Turn ON the selected relay
  const int relayPins[NUM_METERS] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN};
  digitalWrite(relayPins[meterIndex], RELAY_ON);

  activeMeter = meterIndex;
  Serial.printf("[RELAY] Meter %d ON\n", activeMeter + 1);
}

// ============================================================
//  allRelaysOff()
// ============================================================
void allRelaysOff() {
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  Serial.println(F("[RELAY] All relays OFF"));
}

// ============================================================
//  nextEnabledMeter()
// ============================================================
int nextEnabledMeter(int current) {
  for (int i = 1; i <= NUM_METERS; i++) {
    int candidate = (current + i) % NUM_METERS;
    if (meters[candidate].enabled) {
      return candidate;
    }
  }
  return current; // no other meter enabled
}



// ============================================================
//  saveSettings()
// ============================================================
void saveSettings() {
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  int addr = EEPROM_LIMITS_ADDR;
  for (int i = 0; i < NUM_METERS; i++) {
    EEPROM.put(addr, meters[i].energyLimit);
    addr += sizeof(float);
  }
  for (int i = 0; i < NUM_METERS; i++) {
    EEPROM.write(EEPROM_ENABLED_ADDR + i, meters[i].enabled ? 1 : 0);
  }
  EEPROM.write(EEPROM_ACTIVE_ADDR,  (uint8_t)activeMeter);
  EEPROM.write(EEPROM_BYPASS_ADDR,  bypassMode ? 1 : 0);
  // Daily statistics
  addr = EEPROM_DAILY_ADDR;
  for (int i = 0; i < 30; i++) {
    EEPROM.put(addr, dailyUsage[i]);
    addr += sizeof(float);
  }
  EEPROM.write(EEPROM_DIDX_ADDR, (uint8_t)constrain(dailyIndex, 0, 29));
  EEPROM.put(EEPROM_TODAY_ADDR,   todayUsed);
  EEPROM.put(EEPROM_CURMON_ADDR,  currentMonthUsed);
  EEPROM.put(EEPROM_LASTMON_ADDR, lastMonthUsed);
  EEPROM.put(EEPROM_OV_ADDR, ovVoltThresh);
  EEPROM.put(EEPROM_UV_ADDR, uvVoltThresh);
  EEPROM.put(EEPROM_OC_ADDR, ocCurrThresh);
  EEPROM.commit();
  Serial.println(F("[EEPROM] Settings saved"));
}

// ============================================================
//  loadSettings()
// ============================================================
void loadSettings() {
  // Default values
  for (int i = 0; i < NUM_METERS; i++) {
    meters[i].energyLimit = 5.0f;
    meters[i].enabled     = true;
    meters[i].energyUsed  = 0.0f;
  }
  activeMeter = 0;
  bypassMode  = false;

  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    Serial.println(F("[EEPROM] No valid data - using defaults"));
    return;
  }

  int addr = EEPROM_LIMITS_ADDR;
  for (int i = 0; i < NUM_METERS; i++) {
    EEPROM.get(addr, meters[i].energyLimit);
    addr += sizeof(float);
    if (isnan(meters[i].energyLimit) || meters[i].energyLimit <= 0) {
      meters[i].energyLimit = 5.0f;
    }
  }
  for (int i = 0; i < NUM_METERS; i++) {
    meters[i].enabled = (EEPROM.read(EEPROM_ENABLED_ADDR + i) == 1);
  }
  activeMeter = constrain(EEPROM.read(EEPROM_ACTIVE_ADDR), 0, NUM_METERS - 1);
  bypassMode  = (EEPROM.read(EEPROM_BYPASS_ADDR) == 1);
  // Daily statistics
  addr = EEPROM_DAILY_ADDR;
  for (int i = 0; i < 30; i++) {
    EEPROM.get(addr, dailyUsage[i]);
    addr += sizeof(float);
    if (isnan(dailyUsage[i]) || dailyUsage[i] < 0) dailyUsage[i] = 0.0f;
  }
  dailyIndex = (int)constrain(EEPROM.read(EEPROM_DIDX_ADDR), 0, 29);
  EEPROM.get(EEPROM_TODAY_ADDR,   todayUsed);
  EEPROM.get(EEPROM_CURMON_ADDR,  currentMonthUsed);
  EEPROM.get(EEPROM_LASTMON_ADDR, lastMonthUsed);
  if (isnan(todayUsed)        || todayUsed < 0)        todayUsed = 0.0f;
  if (isnan(currentMonthUsed) || currentMonthUsed < 0) currentMonthUsed = 0.0f;
  if (isnan(lastMonthUsed)    || lastMonthUsed < 0)    lastMonthUsed = 0.0f;
  // Protection thresholds
  EEPROM.get(EEPROM_OV_ADDR, ovVoltThresh);
  EEPROM.get(EEPROM_UV_ADDR, uvVoltThresh);
  EEPROM.get(EEPROM_OC_ADDR, ocCurrThresh);
  if (isnan(ovVoltThresh) || ovVoltThresh < 220 || ovVoltThresh > 300) ovVoltThresh = 250.0f;
  if (isnan(uvVoltThresh) || uvVoltThresh < 100 || uvVoltThresh > 220) uvVoltThresh = 180.0f;
  if (isnan(ocCurrThresh) || ocCurrThresh < 0.1 || ocCurrThresh > 200) ocCurrThresh = 16.0f;

  Serial.println(F("[EEPROM] Settings loaded"));
}

// ============================================================
//  printSerial()  - periodic debug output
// ============================================================
void printSerial() {
  if (millis() - lastSerialPrint < SERIAL_PRINT_INTERVAL) return;
  lastSerialPrint = millis();

  Serial.println(F("------ STATUS ------"));
  Serial.printf("Voltage  : %.1f V\n",  liveVoltage);
  Serial.printf("Current  : %.2f A\n",  liveCurrent);
  Serial.printf("Power    : %.1f W\n",  livePower);
  Serial.printf("Energy   : %.3f kWh\n",liveEnergy);
  Serial.printf("Active   : Meter %d\n",activeMeter + 1);
  Serial.printf("Relay1   : %s\n", digitalRead(RELAY1_PIN) == RELAY_ON ? "ON" : "OFF");
  Serial.printf("Relay2   : %s\n", digitalRead(RELAY2_PIN) == RELAY_ON ? "ON" : "OFF");
  Serial.printf("Relay3   : %s\n", digitalRead(RELAY3_PIN) == RELAY_ON ? "ON" : "OFF");
  Serial.printf("Mode     : %s\n", emergencyOff ? "EMERGENCY" : (bypassMode ? "BYPASS" : "AUTO"));
  if (protTrip) Serial.printf("PROT     : TRIP - %s\n", protReason.c_str());
  Serial.println(F("--------------------"));
}

// ============================================================
//  WEB API HANDLERS
// ============================================================

// GET /api/status  -> JSON
void handleApiStatus() {
  StaticJsonDocument<1024> doc;
  doc["voltage"]     = pzemOK ? liveVoltage : 0.0f;
  doc["current"]     = pzemOK ? liveCurrent : 0.0f;
  doc["power"]       = pzemOK ? livePower   : 0.0f;
  doc["energy"]      = liveEnergy;
  doc["activeMeter"] = activeMeter;
  doc["bypass"]      = bypassMode;
  doc["emergency"]   = emergencyOff;
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
  for (int i = 0; i < NUM_METERS; i++) {
    lims.add(meters[i].energyLimit);
    enab.add(meters[i].enabled);
    used.add(meters[i].energyUsed);
  }
  // daily[]: oldest entry first, current day last
  for (int i = 0; i < 30; i++) {
    daily.add(dailyUsage[(dailyIndex + i) % 30]);
  }
  // RTC time so dashboard can compute next reset date
  if (rtc.begin()) {
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

// GET /api/setLimits?l0=5.0&l1=5.0&l2=5.0
void handleApiSetLimits() {
  if (server.hasArg("l0")) meters[0].energyLimit = server.arg("l0").toFloat();
  if (server.hasArg("l1")) meters[1].energyLimit = server.arg("l1").toFloat();
  if (server.hasArg("l2")) meters[2].energyLimit = server.arg("l2").toFloat();

  // Clamp to sane range
  for (int i = 0; i < NUM_METERS; i++) {
    if (meters[i].energyLimit <= 0 || meters[i].energyLimit > 9999) {
      meters[i].energyLimit = 5.0f;
    }
  }
  saveSettings();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/setEnabled?idx=0&val=1
void handleApiSetEnabled() {
  if (server.hasArg("idx") && server.hasArg("val")) {
    int idx = server.arg("idx").toInt();
    if (idx >= 0 && idx < NUM_METERS) {
      meters[idx].enabled = (server.arg("val").toInt() == 1);
      saveSettings();
    }
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/switchMeter?m=0
void handleApiSwitchMeter() {
  if (server.hasArg("m")) {
    int m = server.arg("m").toInt();
    if (m >= 0 && m < NUM_METERS && meters[m].enabled && !protTrip) {
      emergencyOff = false;
      pzemEnergyBase = liveEnergy;
      switchToMeter(m);
      saveSettings();
    }
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/resetEnergy
void handleApiResetEnergy() {
  for (int i = 0; i < NUM_METERS; i++) {
    meters[i].energyUsed = 0;
  }
  pzemEnergyBase = liveEnergy; // new baseline
  Serial.println(F("[WEB] Energy reset via dashboard"));
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/emergency
void handleApiEmergency() {
  emergencyOff = true;
  bypassMode   = false;
  allRelaysOff();
  Serial.println(F("[WEB] Emergency OFF via dashboard"));
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/setProtection?ov=250&uv=180&oc=16
void handleApiSetProtection() {
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
  saveSettings();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /api/clearFault
void handleApiClearFault() {
  if (protTrip) {
    protTrip   = false;
    protReason = "";
    Serial.println(F("[PROT] Fault cleared via dashboard"));
    if (!emergencyOff) {
      switchToMeter(activeMeter);
    }
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ============================================================
//  handleProtection()  — over/under voltage & over current
// ============================================================
void handleProtection() {
  if (!pzemOK || protTrip) return;

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
    allRelaysOff();
    Serial.printf("[PROT] TRIP: %s\n", protReason.c_str());
  }
}
