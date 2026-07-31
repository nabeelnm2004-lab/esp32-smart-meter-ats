/*
 * dashboard.h — the single-page dashboard served at "/".
 *
 * The whole UI is one self-contained HTML document (styles and script
 * inline) held in PROGMEM so it costs no RAM until requested. It is
 * meter-count agnostic: it reads meterCount from /api/status and builds
 * each limit row and dropdown option dynamically. GPIO numbers are
 * never sent to the browser.
 *
 * Extracted verbatim from the original monolithic sketch during the
 * move to the src/ module layout — the markup is unchanged, so the
 * firmware and dashboard stay in sync.
 */
#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include <Arduino.h>

namespace ui {

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
.meter-list{display:flex;flex-direction:column;gap:9px;max-height:320px;overflow-y:auto;padding-right:3px}
.limscroll{max-height:320px;overflow-y:auto;padding-right:3px}
.meter-list::-webkit-scrollbar,.limscroll::-webkit-scrollbar,#evList::-webkit-scrollbar,.wmbox::-webkit-scrollbar{width:6px}
.meter-list::-webkit-scrollbar-track,.limscroll::-webkit-scrollbar-track{background:#1a3050;border-radius:4px}
.meter-list::-webkit-scrollbar-thumb,.limscroll::-webkit-scrollbar-thumb{background:#3b82f6;border-radius:4px}
.mi{position:relative;background:var(--card2);border:1px solid var(--border);border-radius:10px;padding:11px 14px;transition:all .3s}
.mi.fade-in{animation:rowIn .3s ease-out}
.mi.fade-out{animation:rowOut .3s ease-out forwards}
@keyframes rowIn{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:translateY(0)}}
@keyframes rowOut{to{opacity:0;transform:translateY(8px);max-height:0;margin:0;padding:0;border-width:0}}
.mrm{position:absolute;top:7px;right:7px;width:22px;height:22px;line-height:20px;text-align:center;background:rgba(239,68,68,.12);border:1px solid #7f1d1d;color:#fca5a5;border-radius:6px;font-size:0.85rem;font-weight:700;cursor:pointer;opacity:.4;transition:opacity .2s,background .2s;padding:0;display:flex;align-items:center;justify-content:center}
.mi:hover .mrm{opacity:1}
.mrm:hover{background:rgba(239,68,68,.3)}
.mi-top{padding-right:26px}
.addmeter{margin-top:4px}
.btn:disabled{opacity:.45;cursor:not-allowed;filter:none;box-shadow:none}
.btn:disabled:hover{transform:none;filter:none}
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
.bdg-warn{background:#1c1100;border-color:#78350f;color:#fde68a;animation:pulseR 1s infinite}
.wmodal{display:none;position:fixed;inset:0;background:rgba(2,6,14,.72);z-index:60;align-items:center;justify-content:center;padding:16px}
.wmodal.show{display:flex}
.wmbox{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:20px;width:100%;max-width:420px;max-height:82vh;overflow-y:auto}
.wmhdr{display:flex;align-items:center;gap:8px;margin-bottom:10px}
.wmtitle{font-size:0.95rem;font-weight:700;color:#fff;flex:1}
.wmbtn{background:#0d1f38;border:1px solid var(--border);color:var(--text);width:30px;height:30px;border-radius:8px;cursor:pointer;font-size:0.95rem;line-height:1}
.wmbtn:hover{background:#123055}
.wmerr{color:#fca5a5;font-size:0.78rem;margin-top:8px;display:none}
.wmerr.show{display:block}
.nrow{display:flex;align-items:center;gap:10px;padding:12px;border-radius:10px}
.nrow:hover{background:#0d1f38}
.nssid{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:0.86rem}
.sbar{display:inline-flex;align-items:flex-end;gap:2px;height:14px}
.sb{width:3px;background:#1a3050;border-radius:1px}
.sb.g{background:var(--green)}.sb.y{background:var(--yellow)}.sb.r{background:var(--red)}
.npwrap{max-height:0;overflow:hidden;transition:max-height .25s ease}
.npwrap.open{max-height:70px}
.wspin{width:26px;height:26px;border:3px solid var(--border);border-top-color:var(--accent);border-radius:50%;animation:spin .8s linear infinite;margin:22px auto}
@keyframes spin{to{transform:rotate(360deg)}}
@media(max-width:480px){.wmodal{padding:0}.wmbox{max-width:100%;height:100%;max-height:100%;border-radius:0}}
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
    <span class="badge bdg-mode" id="bdgWifi" style="display:none"><span class="dot"></span><span id="wifiTxt">WiFi</span></span>
    <span class="badge bdg-conn" id="bdgOta" style="display:none"><span class="dot"></span><span id="otaTxt">OTA Ready</span></span>
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
      <button class="btn btn-blue addmeter" id="addMeterBtn" onclick="addMeter()">&#10133; Add Meter</button>
    </div>
    <div class="card">
      <div class="ctitle">&#9881; Energy Limits</div>
      <div class="limscroll"><div id="limitRows"></div></div>
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
      <div class="ctitle">&#128246; WiFi &amp; Connectivity</div>
      <div class="ctrls">
        <div class="cg">
          <div class="cg-lbl">Status</div>
          <p class="hint" id="wStatus" style="margin-top:8px">--</p>
        </div>
        <div class="cg">
          <div class="cg-lbl">Network</div>
          <button class="btn btn-blue" onclick="openScan()">&#128246; Scan Networks</button>
        </div>
        <div class="cg">
          <div class="cg-lbl">WiFi Mode</div>
          <select class="csel" id="wMode" onchange="saveMode()">
            <option value="0">AP Only (hotspot)</option>
            <option value="1">Station Only (home WiFi)</option>
            <option value="2">AP + Station</option>
          </select>
          <p class="hint">The hotspot stays up until a home-WiFi link is proven, so the dashboard is never lost.</p>
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
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128295; Maintenance</div>
      <div class="ctrls">
        <div class="cg">
          <div class="cg-lbl">Relay Test Mode</div>
          <button class="btn btn-amber" id="tmBtn" onclick="toggleTestMode()">&#128296; Test Mode: OFF</button>
          <div id="tmRelays" style="display:none;margin-top:8px"></div>
          <p class="hint">Scheduler is locked out while testing. Auto-exits after 5 min.</p>
        </div>
        <div class="cg">
          <div class="cg-lbl">Time</div>
          <button class="btn btn-blue" onclick="syncBrowserTime()">&#128336; Use Browser Time</button>
          <p class="hint" id="rtcInfo">--</p>
        </div>
        <div class="cg">
          <div class="cg-lbl">Config Backup</div>
          <button class="btn btn-green" onclick="location.href='/api/backup'">&#11015; Export JSON</button>
          <input type="file" id="restFile" accept=".json" style="display:none" onchange="doRestore(this)">
          <button class="btn btn-amber" onclick="document.getElementById('restFile').click()">&#11014; Import JSON</button>
        </div>
        <div class="cg">
          <div class="cg-lbl">Firmware Update (OTA)</div>
          <input type="file" id="fwFile" accept=".bin" style="display:none" onchange="doOta(this)">
          <button class="btn btn-blue" onclick="document.getElementById('fwFile').click()">&#128190; Upload .bin</button>
          <div class="bwrap" style="margin-top:8px"><div class="bfill" id="otaBar" style="width:0%"></div></div>
          <p class="hint" id="otaMsg">--</p>
        </div>
        <div class="cg">
          <div class="cg-lbl">Danger Zone</div>
          <button class="btn btn-red" onclick="doFactoryReset()">&#9888; Factory Reset</button>
          <p class="hint">Erases ALL settings and reboots with defaults.</p>
        </div>
      </div>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128203; Event Log <button class="btn btn-amber" style="width:auto;float:right;margin:-6px 0 0;padding:5px 12px" onclick="clearEvents()">Clear</button></div>
      <div id="evList" style="max-height:220px;overflow-y:auto;font-size:0.76rem;line-height:1.7;font-family:monospace"></div>
    </div>
    <div class="card" style="grid-column:1/-1">
      <div class="ctitle">&#128187; System Information</div>
      <div class="stats-grid" id="sysGrid" style="grid-template-columns:repeat(auto-fit,minmax(150px,1fr))"></div>
    </div>
  </div>
</main>

<div class="footer" id="accessInfo">Access: http://192.168.4.1/</div>

<div id="toast"></div>
<div class="wmodal" id="lgModal" onclick="if(event.target===this)hideLogin()">
  <div class="wmbox" style="max-width:340px">
    <div class="wmhdr"><span class="wmtitle">&#128274; Device Login</span></div>
    <p class="hint" style="margin:0 0 10px">Enter the device password to enable emergency stop, config import, factory reset and firmware upload.</p>
    <input class="linp" type="password" id="lgPw" maxlength="64" placeholder="Device password" onkeydown="if(event.key==='Enter')doLogin()">
    <button class="btn btn-blue" onclick="doLogin()">&#10003; Unlock</button>
    <p class="wmerr" id="lgErr"></p>
  </div>
</div>
<div class="wmodal" id="wModal" onclick="if(event.target===this)closeScan()">
  <div class="wmbox">
    <div class="wmhdr">
      <span class="wmtitle">Available Networks</span>
      <button class="wmbtn" onclick="scanNets()" title="Refresh">&#8635;</button>
      <button class="wmbtn" onclick="closeScan()" title="Close">&#10005;</button>
    </div>
    <div id="wList"></div>
    <p class="wmerr" id="wErr"></p>
  </div>
</div>
<script>
// N = Active Meters reported by the firmware. Every meter card,
// limit row and dropdown option below is generated from N —
// nothing meter-specific is hard-coded in this page.
var N=0;
var _tt;
// FIX m4: BasicAuth for the mutating routes (/api/emergency, /api/factoryReset,
// /api/restore, /api/update). Credential = OTA password (NVS "otapass"), entered
// once per session in the login modal and sent as an Authorization header.
// fetch()/XHR won't trigger the browser's native 401 dialog and we don't want it
// to — the modal below is the only password UI.
var _auth=null,_authPromise=null,_authResolve=null;
function showLogin(msg){
  var e=document.getElementById('lgErr');
  e.textContent=msg||'';e.className=msg?'wmerr show':'wmerr';
  document.getElementById('lgModal').className='wmodal show';
  document.getElementById('lgPw').focus();
  if(!_authPromise)_authPromise=new Promise(function(f){_authResolve=f;});
  return _authPromise;
}
// Dismissed (unlocked or cancelled) — release anything awaiting the modal.
function hideLogin(){
  document.getElementById('lgModal').className='wmodal';
  if(_authResolve){var f=_authResolve;_authResolve=null;_authPromise=null;f(_auth);}
}
function doLogin(){
  var i=document.getElementById('lgPw');
  if(!i.value){showLogin('Password required');return;}
  _auth='Basic '+btoa('admin:'+i.value);
  i.value='';
  hideLogin();
}
// Header for a protected call — blocks on the login modal until unlocked.
async function authHdr(){
  if(_auth===null)await showLogin();
  return _auth?{'Authorization':_auth}:{};
}
// On a 401 the cached credential was wrong — clear it and re-open the modal.
function checkAuth(r){if(r&&r.status===401){_auth=null;showLogin('Wrong password — try again');return true;}return false;}
function toast(m,t){var e=document.getElementById('toast');e.textContent=m;e.className='show '+(t||'ok');clearTimeout(_tt);_tt=setTimeout(function(){e.className=''},3200);}
function setVal(id,v,d,na){var e=document.getElementById(id);if(na){e.textContent='--';e.className='mc-val na';}else{e.textContent=v.toFixed(d);e.className='mc-val';}}
function buildUI(n,animateLast){
  N=n;
  var i,h;
  // Meter status cards — each row carries its own remove button (×).
  // The button is hidden by updMeters() when the meter is active or
  // when only one meter remains (minimum-1 invariant enforced server-side too).
  h='';
  for(i=0;i<n;i++){
    h+='<div class="mi'+(animateLast&&i===n-1?' fade-in':'')+'" id="mi'+i+'">'
      +'<div class="mrm" id="rm'+i+'" onclick="removeMeter('+i+')" title="Remove Meter '+(i+1)+'">&times;</div>'
      +'<div class="mi-top"><div class="mi-name"><span id="ad'+i+'"></span>Meter '+(i+1)+'</div>'
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
function updMeters(d){
  for(var i=0;i<N;i++){
    var mi=document.getElementById('mi'+i);
    var bar=document.getElementById('bar'+i);
    var us=document.getElementById('us'+i);
    var lm=document.getElementById('lm'+i);
    var mt=document.getElementById('mt'+i);
    var ad=document.getElementById('ad'+i);
    var rm=document.getElementById('rm'+i);
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
    // Remove button: hidden for the active meter or when only one remains.
    if(rm)rm.style.display=(d.activeMeter===i||N<=1)?'none':'flex';
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
  // IMPROVEMENT #6: WiFi station status badge
  var bw=document.getElementById('bdgWifi');
  var wt=document.getElementById('wifiTxt');
  if(d.wifiMode>0){
    bw.style.display='';
    bw.className='badge '+(d.staOK?'bdg-conn':'bdg-warn');
    wt.textContent=d.staOK?('WiFi: Connected'):(d.staGaveUp?'WiFi: Reconnecting':'WiFi: Connecting');
    bw.title=d.staOK?((d.staSsid||'')+(d.rssi?' • '+d.rssi+' dBm':'')):(d.staSsid||'');
  }else{bw.style.display='none';}
  // IMPROVEMENT #7: OTA badge
  var bo=document.getElementById('bdgOta');
  if(d.otaReady){bo.style.display='';bo.className='badge bdg-conn';}
  else{bo.style.display='none';}
  // IMPROVEMENT #5: PZEM counter-reset warning toast (once per event)
  if(d.pzemWasReset){toast('⚠ PZEM energy counter reset detected ('+(d.pzemResetCount||1)+'x since boot)','warn');}
  // IMPROVEMENT #6: WiFi config card status line + initial mode value
  var ws=document.getElementById('wStatus');
  if(ws){
    ws.textContent=d.wifiMode===0?'AP Only — hotspot "SmartMeterATS"'
      :(d.staOK?('Connected to '+(d.staSsid||'?')+' • '+d.staIP)
      :(d.staGaveUp?'Reconnecting to '+(d.staSsid||'?')+'...':'Connecting to '+(d.staSsid||'?')+'...'));
    var wm=document.getElementById('wMode');
    if(!wm.dataset.loaded){wm.value=d.wifiMode||0;wm.dataset.loaded='1';}
  }
}
// WiFi scan modal. Connect is non-blocking: the firmware starts the
// association and we poll /api/wifiStatus for the verdict.
function openScan(){document.getElementById('wModal').className='wmodal show';scanNets();}
function closeScan(){document.getElementById('wModal').className='wmodal';}
function wErr(m){var e=document.getElementById('wErr');e.textContent=m||'';e.className=m?'wmerr show':'wmerr';}
function bars(r){
  // -50+ = 4 green, -67+ = 3 green, -70+ = 2 yellow, else 1 red
  var n=r>=-50?4:r>=-67?3:r>=-70?2:1,c=n>=3?'g':n===2?'y':'r',h='';
  for(var i=1;i<=4;i++)h+='<div class="sb'+(i<=n?' '+c:'')+'" style="height:'+(i*3+3)+'px"></div>';
  return '<span class="sbar" title="'+r+' dBm">'+h+'</span>';
}
async function scanNets(){
  wErr('');
  document.getElementById('wList').innerHTML='<div class="wspin"></div>';
  try{
    var r=await fetch('/api/scanWiFi');
    var d=await r.json();
    var n=d.networks||[];
    if(!n.length){document.getElementById('wList').innerHTML='<p class="hint" style="padding:12px">No networks found.</p>';return;}
    var h='';
    for(var i=0;i<n.length;i++){
      var s=n[i],e=s.encrypted;
      h+='<div class="nrow">'+bars(s.rssi)+'<span>'+(e?'&#128274;':'&#128275;')+'</span>'
        +'<span class="nssid" id="ns'+i+'"></span>'
        +'<button class="btn btn-blue" style="width:auto;margin:0;padding:6px 14px" id="nb'+i+'" onclick="pick('+i+','+(e?1:0)+')">Connect</button></div>'
        +'<div class="npwrap" id="np'+i+'"><input class="linp" type="password" maxlength="64" id="pw'+i+'" placeholder="Password" style="margin:0 12px 10px"></div>';
    }
    document.getElementById('wList').innerHTML=h;
    // textContent, never innerHTML — an SSID may contain markup
    for(var j=0;j<n.length;j++)document.getElementById('ns'+j).textContent=n[j].ssid;
    _nets=n;
  }catch(ex){document.getElementById('wList').innerHTML='';wErr('Scan failed — try again.');}
}
var _nets=[];
function pick(i,enc){
  var b=document.getElementById('nb'+i),w=document.getElementById('np'+i);
  if(!enc)return doConnect(i,'');
  if(w.className.indexOf('open')<0){w.className='npwrap open';b.textContent='Confirm';document.getElementById('pw'+i).focus();return;}
  doConnect(i,document.getElementById('pw'+i).value);
}
async function doConnect(i,pass){
  var b=document.getElementById('nb'+i);
  wErr('');b.textContent='...';b.disabled=true;
  try{
    var m=document.getElementById('wMode').value;
    if(m==='0')m='2';   // AP-only can't hold a station link — go AP+STA
    var r=await fetch('/api/connectWiFi?mode='+m+'&ssid='+encodeURIComponent(_nets[i].ssid)
      +'&pass='+encodeURIComponent(pass));
    var d=await r.json();
    if(d.status!=='ok'){wErr(d.msg||'Connection failed');b.textContent='Connect';b.disabled=false;return;}
    // Association runs in the background; poll for the verdict.
    for(var t=0;t<32;t++){
      await new Promise(function(f){setTimeout(f,1000);});
      var s=await(await fetch('/api/wifiStatus')).json();
      if(s.staOK){toast('Connected to '+s.ssid+' — '+s.staIP,'ok');closeScan();return;}
      if(s.err){wErr(s.err);b.textContent='Connect';b.disabled=false;return;}
    }
    wErr('Connection timeout');b.textContent='Connect';b.disabled=false;
  }catch(ex){wErr('Connection failed');b.textContent='Connect';b.disabled=false;}
}
async function saveMode(){
  var m=document.getElementById('wMode').value;
  try{
    var d=await(await fetch('/api/setWiFi?mode='+m)).json();
    toast(d.status==='ok'?'WiFi mode saved — applying...':(d.msg||'Error'),d.status==='ok'?'ok':'err');
  }catch(ex){toast('Error saving WiFi mode','err');}
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
  var rd=d.resetDay||1;
  var nr=(now.getDate()<rd)?new Date(now.getFullYear(),now.getMonth(),rd):new Date(now.getFullYear(),now.getMonth()+1,rd);
  document.getElementById('stNext').textContent=mn[nr.getMonth()]+' '+rd+', '+nr.getFullYear();
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
    var ab=document.getElementById('addMeterBtn');
    if(ab)ab.disabled=(d.meterCount>=(d.maxMeters||10));
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
    updTestMode(d);
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
  var r=await fetch('/api/emergency',{headers:await authHdr()});
  if(checkAuth(r)){toast('Auth required — try again','err');return;}
  var d=await r.json();
  toast(d.status==='ok'?'Emergency OFF activated!':(d.msg||'Error'),'err');
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
async function addMeter(){
  var r=await fetch('/api/addMeter');
  var d=await r.json();
  if(d.status==='ok'){
    toast('Meter '+(d.idx+1)+' added','ok');
    buildUI(d.meterCount,true);
    poll();
  }else toast(d.msg||'Cannot add meter','err');
}
async function removeMeter(i){
  if(!confirm('Remove Meter '+(i+1)+'? Its usage data will be lost.'))return;
  var row=document.getElementById('mi'+i);
  if(row){row.classList.add('fade-out');}
  var r=await fetch('/api/removeMeter?idx='+i);
  var d=await r.json();
  if(d.status==='ok'){
    toast('Meter removed','ok');
    buildUI(d.meterCount);
    poll();
  }else{
    if(row)row.classList.remove('fade-out');
    toast(d.msg||'Cannot remove meter','err');
  }
}
// ===== FEATURE 12: relay test mode =====
var tmOn=false;
function updTestMode(d){
  tmOn=!!d.testMode;
  var b=document.getElementById('tmBtn');
  var r=document.getElementById('tmRelays');
  b.innerHTML='&#128296; Test Mode: '+(tmOn?'ON ('+(d.testLeft||0)+'s left)':'OFF');
  b.className='btn '+(tmOn?'btn-red':'btn-amber');
  if(tmOn&&!r.dataset.built){
    var h='';
    for(var i=0;i<N;i++){h+='<button class="btn btn-blue" style="margin-top:4px" onclick="testRelay('+i+')">Toggle Relay '+(i+1)+'</button>';}
    r.innerHTML=h;r.dataset.built='1';
  }
  r.style.display=tmOn?'':'none';
  if(!tmOn)delete r.dataset.built;
}
async function toggleTestMode(){
  if(!tmOn&&!confirm('Enable RELAY TEST MODE?\n\nThe scheduler is disabled and relays switch ONLY by your button presses. Loads WILL be energised/de-energised. Continue?'))return;
  var r=await fetch('/api/testMode?on='+(tmOn?0:1));
  var d=await r.json();
  toast(d.status==='ok'?('Test mode '+(tmOn?'disabled':'enabled')):'Error','warn');
}
async function testRelay(i){
  var r=await fetch('/api/testRelay?idx='+i);
  var d=await r.json();
  toast(d.status==='ok'?('Relay '+(i+1)+' '+(d.on?'ON':'OFF')):(d.msg||'Error'),d.status==='ok'?'ok':'err');
}
// ===== FEATURE 14: browser time sync =====
async function syncBrowserTime(){
  var n=new Date();
  var q='/api/setTime?y='+n.getFullYear()+'&mo='+(n.getMonth()+1)+'&d='+n.getDate()
       +'&h='+n.getHours()+'&mi='+n.getMinutes()+'&s='+n.getSeconds();
  var r=await fetch(q);
  var d=await r.json();
  toast(d.status==='ok'?'Time synced from browser':'Time sync failed',d.status==='ok'?'ok':'err');
  poll();loadSys();
}
// ===== FEATURE 8: backup / restore =====
async function doRestore(inp){
  var f=inp.files[0];if(!f)return;
  var txt=await f.text();inp.value='';
  try{JSON.parse(txt);}catch(e){toast('Not a valid JSON file','err');return;}
  if(!confirm('Import this configuration? Current settings will be overwritten.'))return;
  var r=await fetch('/api/restore',{method:'POST',headers:Object.assign({'Content-Type':'application/json'},await authHdr()),body:txt});
  if(checkAuth(r)){toast('Auth required — try again','err');return;}
  var d=await r.json();
  toast(d.status==='ok'?'Config restored!':(d.msg||'Restore failed'),d.status==='ok'?'ok':'err');
  if(d.status==='ok')setTimeout(poll,500);
}
// ===== FEATURE 9: factory reset =====
async function doFactoryReset(){
  if(!confirm('FACTORY RESET?\n\nALL settings, statistics and WiFi credentials will be erased and the device will reboot with defaults.'))return;
  if(!confirm('Are you REALLY sure? This cannot be undone.'))return;
  var r=await fetch('/api/factoryReset',{headers:await authHdr()});
  if(checkAuth(r)){toast('Auth required — try again','err');return;}
  toast('Factory reset — device rebooting...','warn');
}
// ===== FEATURE 7: browser OTA upload =====
async function doOta(inp){
  var f=inp.files[0];if(!f)return;
  inp.value='';
  if(!confirm('Flash "'+f.name+'" ('+Math.round(f.size/1024)+' KB)?\nDevice reboots automatically on success.'))return;
  var bar=document.getElementById('otaBar');
  var msg=document.getElementById('otaMsg');
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/update');
  var ah=await authHdr();if(ah.Authorization)xhr.setRequestHeader('Authorization',ah.Authorization);
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);bar.style.width=p+'%';msg.textContent='Uploading... '+p+'%';}};
  xhr.onload=function(){
    if(xhr.status===401){checkAuth({status:401});bar.style.width='0%';msg.textContent='Auth required — retry';toast('Auth required — try again','err');return;}
    try{var d=JSON.parse(xhr.responseText);}catch(e){var d={};}
    if(xhr.status===200&&d.status==='ok'){msg.textContent='Success! Rebooting...';toast('Firmware updated — rebooting','ok');}
    else{bar.style.width='0%';msg.textContent='Failed: '+(d.msg||'error');toast('OTA failed: '+(d.msg||'error'),'err');}
  };
  xhr.onerror=function(){bar.style.width='0%';msg.textContent='Upload error';toast('OTA upload error','err');};
  var fd=new FormData();fd.append('firmware',f,f.name);
  xhr.send(fd);
}
// ===== FEATURE 10: event log viewer =====
async function loadEvents(){
  try{
    var r=await fetch('/api/events');
    var d=await r.json();
    var h='';
    for(var i=d.events.length-1;i>=0;i--){var ev=d.events[i];h+='<div><span style="color:#3d5a7a">'+ev.t+'</span>  '+ev.m+'</div>';}
    document.getElementById('evList').innerHTML=h||'<div style="color:#3d5a7a">No events</div>';
  }catch(e){}
}
async function clearEvents(){
  await fetch('/api/clearEvents');
  toast('Event log cleared','warn');loadEvents();
}
// ===== FEATURE 11/13: system info + RTC health (auto-refresh) =====
function sysItem(l,v){return '<div class="stat-item"><div class="stat-val" style="font-size:0.95rem">'+v+'</div><div class="stat-lbl">'+l+'</div></div>';}
async function loadSys(){
  try{
    var r=await fetch('/api/sysinfo');
    var d=await r.json();
    var up=d.uptime,dd=Math.floor(up/86400),hh=Math.floor(up%86400/3600),mm=Math.floor(up%3600/60);
    document.getElementById('sysGrid').innerHTML=
      sysItem('Firmware','v'+d.fw)+sysItem('Build',d.build)+sysItem('Chip',d.chip)
      +sysItem('Flash',(d.flash/1048576)+' MB')+sysItem('Free Heap',Math.round(d.heap/1024)+' KB')
      +sysItem('CPU',d.cpu+' MHz')+sysItem('Uptime',dd+'d '+hh+'h '+mm+'m')
      +sysItem('Restart Reason',d.rstReason)+sysItem('WiFi RSSI',d.rssi?d.rssi+' dBm':'--')
      +sysItem('IP',d.ip||'--')+sysItem('Time Source',d.timeSrc)+sysItem('Active Meters',d.meterCount);
    document.getElementById('accessInfo').textContent=d.hostname?'Access: http://'+d.hostname+'/':'Access: http://'+d.ip+'/';
    document.getElementById('rtcInfo').textContent=
      (d.rtcOK?('RTC OK'+(d.rtcLostPower?' (battery LOW — time was lost!)':' (battery OK)')+' • '+d.rtcTime):'RTC MISSING — check wiring')
      +(d.lastSync?' • Last sync: '+d.lastSync:'');
  }catch(e){}
}
showLogin();      // unlock up front; dismissable — read-only pages work locked
poll();
setInterval(poll,3000);
loadEvents();loadSys();
setInterval(loadEvents,10000);
setInterval(loadSys,10000);
</script>
</body>
</html>
)rawhtml";

}  // namespace ui

#endif  // UI_DASHBOARD_H