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
:root{--bg:#121212;--panel:#1e1e1e;--border:rgba(255,255,255,.07);--text:#f3f4f6;--muted:#9ca3af;--accent:#3b82f6;--cyan:#06b6d4;--green:#22c55e;--yellow:#eab308;--amber:#f59e0b;--red:#ef4444;--orange:#f97316}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,'Segoe UI',Roboto,system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;-webkit-font-smoothing:antialiased}
::-webkit-scrollbar{width:6px;height:6px}::-webkit-scrollbar-track{background:#121212}::-webkit-scrollbar-thumb{background:#2a2a2a;border-radius:4px}::-webkit-scrollbar-thumb:hover{background:var(--accent)}
.body-bg{position:fixed;inset:0;background:radial-gradient(circle at 20% 10%,rgba(59,130,246,.18),transparent 24%),radial-gradient(circle at 80% 20%,rgba(6,182,212,.12),transparent 20%),radial-gradient(circle at 50% 90%,rgba(245,158,11,.08),transparent 24%),var(--bg);pointer-events:none;z-index:-1}
.app{display:flex;min-height:100vh}
.side{width:248px;background:rgba(255,255,255,.03);border-right:1px solid var(--border);display:flex;flex-direction:column;position:sticky;top:0;height:100vh;overflow-y:auto;flex-shrink:0;backdrop-filter:blur(12px)}
.brand{padding:22px 20px 16px;display:flex;align-items:center;gap:12px}
.brand-ic{width:38px;height:38px;border-radius:12px;background:linear-gradient(135deg,#2563eb,#06b6d4);display:flex;align-items:center;justify-content:center;font-size:1.15rem;box-shadow:0 10px 24px rgba(37,99,235,.35);flex-shrink:0}
.brand-t{font-weight:800;font-size:.98rem;color:#fff;line-height:1.1;letter-spacing:-.02em}
.brand-s{font-size:.6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.12em;font-weight:700;margin-top:3px;font-family:ui-monospace,monospace}
.nav{flex:1;padding:8px 14px;display:flex;flex-direction:column;gap:5px}
.navbtn{display:flex;align-items:center;gap:12px;padding:11px 14px;border-radius:12px;background:none;border:1px solid transparent;color:var(--muted);font-size:.85rem;font-weight:600;cursor:pointer;text-align:left;transition:all .18s;width:100%;font-family:inherit}
.navbtn:hover{color:#fff;background:rgba(255,255,255,.05)}
.navbtn.active{background:rgba(59,130,246,.12);color:#fff;border-color:rgba(59,130,246,.22)}
.navbtn .ic{font-size:1rem;width:20px;text-align:center;flex-shrink:0}
.side-foot{padding:16px}
.conn-w{background:rgba(255,255,255,.04);border:1px solid var(--border);border-radius:16px;padding:14px;box-shadow:0 8px 24px rgba(0,0,0,.18)}
.conn-h{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}
.conn-l{font-size:.58rem;color:var(--muted);text-transform:uppercase;letter-spacing:.14em;font-weight:800}
.main{flex:1;min-width:0;display:flex;flex-direction:column}
.topbar{position:sticky;top:0;z-index:40;background:rgba(5,8,20,.92);backdrop-filter:blur(18px);border-bottom:1px solid var(--border);padding:0 24px;height:74px;display:flex;align-items:center;justify-content:space-between;gap:14px;box-shadow:0 12px 40px rgba(0,0,0,.22)}
.tb-title{font-size:1.15rem;font-weight:800;color:#fff;letter-spacing:-.02em;line-height:1.2}
.tb-sub{font-size:.72rem;color:var(--muted);margin-top:1px}
.tb-right{display:flex;align-items:center;gap:16px}
.tb-clock{text-align:right}
.tb-clock .t{font-size:.82rem;color:#fff;font-weight:600;font-family:ui-monospace,monospace}
.tb-clock .d{font-size:.68rem;color:var(--muted)}
.badges{display:flex;gap:7px;flex-wrap:wrap}
.badge{display:inline-flex;align-items:center;gap:5px;padding:5px 11px;border-radius:20px;font-size:.68rem;font-weight:700;border:1px solid transparent;letter-spacing:.02em}
.bdg-conn{background:rgba(34,197,94,.1);border-color:rgba(34,197,94,.25);color:#86efac}
.bdg-conn.off{background:rgba(239,68,68,.1);border-color:rgba(239,68,68,.3);color:#fca5a5}
.bdg-mode{background:rgba(59,130,246,.1);border-color:rgba(59,130,246,.25);color:#93c5fd}
.bdg-mode.bypass{background:rgba(245,158,11,.12);border-color:rgba(245,158,11,.3);color:#fde68a}
.bdg-mode.emg{background:rgba(239,68,68,.12);border-color:rgba(239,68,68,.35);color:#fca5a5;animation:pulseR 1s infinite}
.bdg-mode.fault{background:rgba(239,68,68,.14);border-color:#dc2626;color:#fca5a5;animation:pulseR .8s infinite}
.bdg-warn{background:rgba(245,158,11,.12);border-color:rgba(245,158,11,.3);color:#fde68a;animation:pulseR 1s infinite}
.dot{width:7px;height:7px;border-radius:50%;background:currentColor}
@keyframes pulseR{0%,100%{opacity:1}50%{opacity:.4}}
.mtabs{display:none}
.hero{position:relative;overflow:hidden;background:linear-gradient(135deg,rgba(255,255,255,.05),rgba(255,255,255,.02));border:1px solid var(--border);border-radius:22px;padding:18px 20px;margin-bottom:16px;box-shadow:0 10px 30px rgba(0,0,0,.2)}
.hero::before{content:'';position:absolute;inset:auto -10% -55% auto;width:220px;height:220px;border-radius:50%;background:radial-gradient(circle,rgba(59,130,246,.25),transparent 70%);pointer-events:none}
.hero::after{content:'';position:absolute;inset:-30% auto auto -8%;width:180px;height:180px;border-radius:50%;background:radial-gradient(circle,rgba(6,182,212,.18),transparent 68%);pointer-events:none}
.hero-row{display:flex;align-items:center;justify-content:space-between;gap:16px;position:relative;z-index:1}
.hero-kicker{font-size:.62rem;color:var(--muted);text-transform:uppercase;letter-spacing:.14em;font-weight:800;margin-bottom:6px}
.hero-title{font-size:1.45rem;font-weight:800;letter-spacing:-.03em;line-height:1.1}
.hero-sub{font-size:.76rem;color:var(--muted);margin-top:5px;max-width:60ch}
.hero-chip{display:inline-flex;align-items:center;gap:7px;padding:7px 12px;border-radius:999px;background:rgba(255,255,255,.05);border:1px solid var(--border);font-size:.7rem;color:#fff;font-weight:700;white-space:nowrap}
.card.soft{background:rgba(255,255,255,.07)}
.content{padding:24px;max-width:1180px;margin:0 auto;width:100%}
.tabpane{display:none;animation:fadeIn .25s ease}
.tabpane.show{display:block}
@keyframes fadeIn{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:translateY(0)}}
.grid{display:grid;gap:16px}
.g2{grid-template-columns:1fr 1fr}
.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:16px;margin-bottom:18px}
.mc{background:var(--panel);border:1px solid var(--border);border-radius:16px;padding:16px 18px;position:relative;overflow:hidden;transition:border-color .2s,transform .2s;box-shadow:0 8px 24px rgba(0,0,0,.28)}
.mc:hover{transform:translateY(-2px)}
.mc::before{content:'';position:absolute;top:0;left:0;right:0;height:3px}
.mc.v::before{background:linear-gradient(90deg,#3b82f6,#06b6d4)}.mc.v:hover{border-color:rgba(59,130,246,.4)}
.mc.a::before{background:linear-gradient(90deg,#06b6d4,#2563eb)}.mc.a:hover{border-color:rgba(6,182,212,.4)}
.mc.w::before{background:linear-gradient(90deg,#f59e0b,#fbbf24)}.mc.w:hover{border-color:rgba(245,158,11,.4)}
.mc.e::before{background:linear-gradient(90deg,#22c55e,#2dd4bf)}.mc.e:hover{border-color:rgba(34,197,94,.4)}
.mc-hd{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.mc-lbl{font-size:.66rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;font-weight:700}
.mc-icon{width:32px;height:32px;border-radius:11px;display:flex;align-items:center;justify-content:center;font-size:.95rem}
.mc.v .mc-icon{background:rgba(59,130,246,.12)}.mc.a .mc-icon{background:rgba(6,182,212,.12)}.mc.w .mc-icon{background:rgba(245,158,11,.12)}.mc.e .mc-icon{background:rgba(34,197,94,.12)}
.mc-val{font-size:1.9rem;font-weight:800;color:#fff;line-height:1;font-family:ui-monospace,monospace;letter-spacing:-.02em}
.mc-val.na{color:var(--muted);font-size:1.5rem}
.mc-unit{font-size:.68rem;color:var(--muted);font-weight:600;margin-top:6px}
.card{background:var(--panel);border:1px solid var(--border);border-radius:18px;padding:20px;box-shadow:0 8px 24px rgba(0,0,0,.22);margin-bottom:16px}
.card:last-child{margin-bottom:0}
.ctitle{font-size:.68rem;text-transform:uppercase;letter-spacing:.1em;color:var(--muted);font-weight:800;margin-bottom:16px;display:flex;align-items:center;gap:8px}
.meter-list{display:flex;flex-direction:column;gap:10px;max-height:360px;overflow-y:auto;padding-right:3px}
.limscroll{max-height:360px;overflow-y:auto;padding-right:3px}
.mi{position:relative;background:rgba(255,255,255,.03);border:1px solid var(--border);border-radius:14px;padding:13px 15px;transition:all .3s}
.mi.fade-in{animation:rowIn .3s ease-out}
.mi.fade-out{animation:rowOut .3s ease-out forwards}
@keyframes rowIn{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:translateY(0)}}
@keyframes rowOut{to{opacity:0;transform:translateY(8px);max-height:0;margin:0;padding:0;border-width:0}}
.mrm{position:absolute;top:8px;right:8px;width:22px;height:22px;background:rgba(239,68,68,.12);border:1px solid rgba(239,68,68,.4);color:#fca5a5;border-radius:7px;font-size:.85rem;font-weight:700;cursor:pointer;opacity:.4;transition:opacity .2s,background .2s;display:flex;align-items:center;justify-content:center;font-family:inherit;padding:0}
.mi:hover .mrm{opacity:1}
.mrm:hover{background:rgba(239,68,68,.3)}
.addmeter{margin-top:6px}
.btn:disabled{opacity:.45;cursor:not-allowed;box-shadow:none}
.btn:disabled:hover{transform:none;filter:none}
.mi.on{border-color:rgba(59,130,246,.5);background:rgba(59,130,246,.08);box-shadow:0 0 22px rgba(59,130,246,.14)}
.mi.dim{opacity:.42}
.mi-top{display:flex;align-items:center;justify-content:space-between;margin-bottom:9px;padding-right:26px}
.mi-name{font-weight:700;font-size:.86rem;color:#fff;display:flex;align-items:center;gap:8px}
.adot{width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 8px var(--accent);animation:blink 1.5s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.mt{font-size:.62rem;padding:3px 9px;border-radius:20px;font-weight:800;letter-spacing:.04em}
.mt.act{background:rgba(59,130,246,.18);color:#93c5fd}.mt.stby{background:rgba(255,255,255,.06);color:#94a3b8}.mt.dis{background:rgba(255,255,255,.04);color:#64748b}
.bwrap{background:rgba(0,0,0,.35);border-radius:6px;height:7px;overflow:hidden}
.bfill{height:100%;border-radius:6px;transition:width .7s ease;background:linear-gradient(90deg,var(--accent),var(--cyan))}
.bfill.warn{background:linear-gradient(90deg,var(--amber),var(--yellow))}
.bfill.danger{background:linear-gradient(90deg,var(--red),var(--orange))}
.mstats{display:flex;justify-content:space-between;font-size:.68rem;color:var(--muted);margin-top:6px;font-family:ui-monospace,monospace}
.lrow{margin-bottom:13px}
.lhdr{display:flex;align-items:center;justify-content:space-between;margin-bottom:6px}
.llbl{font-size:.8rem;color:var(--text);font-weight:600}
.toggle{position:relative;width:40px;height:22px;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:rgba(255,255,255,.08);border-radius:22px;cursor:pointer;transition:.3s;border:1px solid var(--border)}
.slider:before{content:'';position:absolute;width:16px;height:16px;left:2px;top:2px;background:#64748b;border-radius:50%;transition:.3s}
input:checked+.slider{background:var(--accent);border-color:var(--accent)}
input:checked+.slider:before{transform:translateX(18px);background:#fff}
.linp{width:100%;padding:9px 12px;background:var(--bg);border:1px solid var(--border);color:#fff;border-radius:11px;font-size:.85rem;outline:none;transition:border .2s;font-family:ui-monospace,monospace}
.linp:focus{border-color:var(--accent)}
select.csel{width:100%;padding:10px 12px;background:var(--bg);border:1px solid var(--border);color:#fff;border-radius:11px;font-size:.82rem;outline:none;cursor:pointer;font-family:ui-monospace,monospace}
select.csel:focus{border-color:var(--accent)}
.btn{width:100%;padding:11px;border:none;border-radius:11px;font-size:.82rem;font-weight:700;cursor:pointer;transition:all .2s;margin-top:8px;display:flex;align-items:center;justify-content:center;gap:6px;letter-spacing:.01em}
.btn:hover{transform:translateY(-1px);filter:brightness(1.08)}
.btn:active{transform:translateY(0)}
.btn-blue{background:linear-gradient(135deg,#2563eb,#4f46e5);color:#fff;box-shadow:0 6px 16px rgba(37,99,235,.28)}
.btn-green{background:linear-gradient(135deg,#059669,#10b981);color:#fff;box-shadow:0 6px 16px rgba(5,150,105,.22)}
.btn-amber{background:rgba(245,158,11,.14);color:#fcd34d;border:1px solid rgba(245,158,11,.3)}
.btn-amber:hover{background:rgba(245,158,11,.22)}
.btn-red{background:linear-gradient(135deg,#dc2626,#e11d48);color:#fff;box-shadow:0 6px 16px rgba(220,38,38,.28)}
.ctrls{display:grid;grid-template-columns:repeat(auto-fit,minmax(185px,1fr));gap:14px;align-items:start}
.cg-lbl{font-size:.7rem;color:var(--muted);margin-bottom:6px;font-weight:600}
.hint{font-size:.68rem;color:var(--muted);margin-top:8px;line-height:1.55}
#toast{position:fixed;bottom:22px;right:22px;padding:12px 18px;border-radius:14px;font-size:.82rem;font-weight:600;color:#fff;opacity:0;transform:translateY(10px);transition:all .3s;z-index:999;pointer-events:none;max-width:300px;border:1px solid transparent;box-shadow:0 12px 32px rgba(0,0,0,.5)}
#toast.show{opacity:1;transform:translateY(0)}
#toast.ok{background:rgba(5,46,22,.96);border-color:#166534}
#toast.warn{background:rgba(66,32,6,.96);border-color:#92400e}
#toast.err{background:rgba(69,10,10,.96);border-color:#991b1b}
.net-err{display:flex;align-items:center;gap:12px;flex-wrap:wrap;background:rgba(69,10,10,.28);border:1px solid rgba(239,68,68,.45);color:#fca5a5;border-radius:14px;padding:12px 16px;margin-bottom:16px;font-size:.8rem;font-weight:600}
.net-err button{margin-left:auto}
.stats-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:14px}
.stat-item{background:rgba(255,255,255,.03);border:1px solid var(--border);border-radius:14px;padding:16px;text-align:center}
.stat-icon{font-size:1.35rem;margin-bottom:8px}
.stat-val{font-size:1.4rem;font-weight:800;color:#fff;font-family:ui-monospace,monospace}
.stat-unit{font-size:.66rem;color:var(--muted);font-weight:600;margin-top:2px}
.stat-lbl{font-size:.62rem;color:var(--muted);text-transform:uppercase;letter-spacing:.07em;margin-top:6px}
.graph-wrap{display:flex;align-items:flex-end;gap:3px;height:100px;border-bottom:1px solid var(--border);margin-bottom:6px}
.gb{flex:1;min-width:0;border-radius:3px 3px 0 0;background:linear-gradient(180deg,#3b82f6,#1e3a8a);position:relative;transition:height .5s ease;cursor:pointer}
.gb.today{background:linear-gradient(180deg,var(--cyan),#0e7490)}
.gb:hover::after{content:attr(data-v);position:absolute;top:-24px;left:50%;transform:translateX(-50%);font-size:.6rem;color:#fff;background:#1e293b;padding:3px 6px;border-radius:5px;white-space:nowrap;pointer-events:none;z-index:10}
.graph-lbl{display:flex;justify-content:space-between;font-size:.62rem;color:var(--muted);padding:0 2px}
.footer{text-align:center;padding:14px;font-size:.68rem;color:var(--muted)}
.fault-banner{background:linear-gradient(135deg,rgba(59,7,7,.9),rgba(96,0,0,.7));border:1px solid rgba(239,68,68,.5);border-radius:18px;padding:16px 20px;margin-bottom:18px;display:none;align-items:center;gap:16px;flex-wrap:wrap;box-shadow:0 12px 32px rgba(220,38,38,.18);animation:pulseR 2s infinite}
.fault-banner.show{display:flex}
.fb-icon{width:52px;height:52px;border-radius:14px;background:rgba(239,68,68,.18);border:1px solid rgba(239,68,68,.4);display:flex;align-items:center;justify-content:center;font-size:1.6rem;flex-shrink:0}
.fb-body{flex:1;min-width:180px}
.fb-title{font-size:.9rem;font-weight:800;color:#fecaca;margin-bottom:4px;letter-spacing:.01em}
.fb-reason{font-size:.74rem;color:#fca5a5;opacity:.85;font-family:ui-monospace,monospace}
.prot-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:14px}
.pcard{background:rgba(255,255,255,.03);border:1px solid var(--border);border-radius:14px;padding:14px}
.pcard .cg-lbl{margin-bottom:8px}
.safe-banner{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:14px 18px;background:rgba(5,46,22,.35);border:1px solid rgba(34,197,94,.3);border-radius:14px;margin-bottom:16px}
.safe-banner .sb-l{display:flex;align-items:center;gap:12px}
.safe-banner .sb-ic{font-size:1.4rem}
.safe-banner .sb-t{font-size:.76rem;font-weight:800;color:#86efac;text-transform:uppercase;letter-spacing:.06em}
.safe-banner .sb-s{font-size:.68rem;color:rgba(134,239,172,.75);font-family:ui-monospace,monospace}
.safe-banner .sb-tag{font-size:.64rem;font-family:ui-monospace,monospace;color:#86efac;font-weight:700;padding:4px 10px;border-radius:20px;background:rgba(34,197,94,.12);border:1px solid rgba(34,197,94,.25)}
.wmodal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.72);backdrop-filter:blur(4px);z-index:60;align-items:center;justify-content:center;padding:16px}
.wmodal.show{display:flex}
.wmbox{background:var(--panel);border:1px solid var(--border);border-radius:20px;padding:22px;width:100%;max-width:420px;max-height:82vh;overflow-y:auto;box-shadow:0 24px 60px rgba(0,0,0,.6)}
.wmhdr{display:flex;align-items:center;gap:8px;margin-bottom:12px}
.wmtitle{font-size:.95rem;font-weight:800;color:#fff;flex:1}
.wmbtn{background:rgba(255,255,255,.05);border:1px solid var(--border);color:var(--text);width:32px;height:32px;border-radius:10px;cursor:pointer;font-size:.95rem;line-height:1}
.wmbtn:hover{background:rgba(255,255,255,.1)}
.wmerr{color:#fca5a5;font-size:.78rem;margin-top:8px;display:none}
.wmerr.show{display:block}
.nrow{display:flex;align-items:center;gap:10px;padding:12px;border-radius:12px}
.nrow:hover{background:rgba(255,255,255,.05)}
.nssid{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:.85rem}
.sbar{display:inline-flex;align-items:flex-end;gap:2px;height:14px}
.sb{width:3px;background:rgba(255,255,255,.12);border-radius:1px}
.sb.g{background:var(--green)}.sb.y{background:var(--yellow)}.sb.r{background:var(--red)}
.npwrap{max-height:0;overflow:hidden;transition:max-height .25s ease}
.npwrap.open{max-height:70px}
.wspin{width:26px;height:26px;border:3px solid var(--border);border-top-color:var(--accent);border-radius:50%;animation:spin .8s linear infinite;margin:22px auto}
@keyframes spin{to{transform:rotate(360deg)}}
@media(max-width:960px){
 .side{display:none}
 .metrics{grid-template-columns:repeat(2,1fr)}
 .g2{grid-template-columns:1fr}
 .topbar{padding:0 16px;height:64px}
 .tb-clock{display:none}
 .mtabs{display:flex;gap:6px;overflow-x:auto;padding:10px 14px;border-bottom:1px solid var(--border);background:rgba(5,8,20,.95);position:sticky;top:64px;z-index:39;backdrop-filter:blur(18px)}
 .mtab{flex-shrink:0;display:flex;align-items:center;gap:6px;padding:8px 13px;border-radius:11px;background:none;border:1px solid transparent;color:var(--muted);font-size:.76rem;font-weight:700;cursor:pointer;white-space:nowrap;font-family:inherit}
 .mtab.active{background:rgba(59,130,246,.12);color:var(--accent);border-color:rgba(59,130,246,.22)}
 .content{padding:16px}
 .hero-row{flex-direction:column;align-items:flex-start}
}
@media(max-width:520px){.metrics{grid-template-columns:1fr}.stats-grid{grid-template-columns:repeat(2,1fr)}.wmodal{padding:0}.wmbox{max-width:100%;height:100%;max-height:100%;border-radius:0}}
/* Mobile: no horizontal overflow, roomy tap targets, wrapping badges. */
html,body{max-width:100%;overflow-x:hidden}
@media(max-width:960px){.mtab,.navbtn{padding:11px 14px}}
@media(max-width:520px){.hero-chip{white-space:normal}.badges{gap:5px}.tb-right{gap:8px}.ctitle{flex-wrap:wrap}}
/* Skeleton shimmer while the first /api/status response is in flight. */
.skel{background:linear-gradient(90deg,rgba(255,255,255,.05) 25%,rgba(255,255,255,.12) 50%,rgba(255,255,255,.05) 75%);background-size:200% 100%;animation:skel 1.2s ease-in-out infinite;color:transparent!important;border-radius:6px;min-height:1em}
@keyframes skel{to{background-position:-200% 0}}
/* Connectivity states: connTxt in the header badge, connDot in the sidebar. */
.c-ok{color:var(--green)!important}.c-warn{color:var(--yellow)!important}.c-bad{color:var(--red)!important}
@media(prefers-reduced-motion:reduce){*{animation-duration:.01ms!important;animation-iteration-count:1!important;transition-duration:.01ms!important}}
:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.btn:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.wmbtn:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.linp:focus-visible{outline:2px solid var(--accent);outline-offset:1px}
select.csel:focus-visible{outline:2px solid var(--accent);outline-offset:1px}
.mtab:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
/* Event log — category badges, filter pills, toolbar, error banner */
.ev-toolbar{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:12px}
.ev-filters{display:flex;gap:6px;flex-wrap:wrap}
.evf{padding:6px 12px;border-radius:20px;border:1px solid var(--border);background:none;color:var(--muted);font-size:.68rem;font-weight:700;cursor:pointer;font-family:inherit;transition:all .18s}
.evf:hover{color:#fff;background:rgba(255,255,255,.05)}
.evf.active{background:rgba(59,130,246,.14);color:#93c5fd;border-color:rgba(59,130,246,.35)}
.ev-search{flex:1;min-width:170px;max-width:320px}
.ev-err{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;background:rgba(69,10,10,.28);border:1px solid rgba(239,68,68,.45);color:#fca5a5;border-radius:12px;padding:10px 14px;margin-bottom:10px;font-size:.76rem;font-weight:600}
.ev-err button{width:auto;margin:0;padding:6px 14px}
.ev-list{max-height:340px;overflow-y:auto;padding-right:3px}
.ev-row{display:flex;align-items:flex-start;gap:10px;padding:9px 10px;border-radius:10px;border-bottom:1px solid var(--border)}
.ev-row:hover{background:rgba(255,255,255,.04)}
.ev-badge{flex-shrink:0;display:inline-flex;align-items:center;gap:4px;padding:2px 9px;border-radius:20px;font-size:.6rem;font-weight:800;letter-spacing:.04em;text-transform:uppercase;margin-top:2px}
.ev-info{background:rgba(59,130,246,.12);color:#93c5fd;border:1px solid rgba(59,130,246,.25)}
.ev-warn{background:rgba(245,158,11,.12);color:#fde68a;border:1px solid rgba(245,158,11,.3)}
.ev-fault{background:rgba(239,68,68,.14);color:#fca5a5;border:1px solid rgba(239,68,68,.4);animation:pulseR 1s infinite}
.ev-sec{background:rgba(249,115,22,.12);color:#fdba74;border:1px solid rgba(249,115,22,.3)}
.ev-sys{background:rgba(6,182,212,.12);color:#67e8f9;border:1px solid rgba(6,182,212,.3)}
.ev-main{flex:1;min-width:0}
.ev-desc{font-size:.74rem;line-height:1.6;color:var(--text);overflow-wrap:anywhere}
.ev-meta{display:inline-block;font-size:.62rem;color:var(--muted);margin-top:4px;font-family:ui-monospace,monospace}
.ev-ts{flex-shrink:0;text-align:right;font-size:.62rem;color:var(--muted);font-family:ui-monospace,monospace;line-height:1.5}
.ev-date{display:block}
.ev-time{display:block;color:#94a3b8;font-weight:600}
.ev-empty{padding:26px 12px;text-align:center;color:var(--muted);font-size:.76rem}
</style>
</head>
<body>
<div class="body-bg"></div>
<div class="app">
  <aside class="side">
    <div class="brand">
      <div class="brand-ic">&#9889;</div>
      <div>
        <div class="brand-t">Smart Meter ATS</div>
        <div class="brand-s">Eng-Nabeel</div>
      </div>
    </div>
    <nav class="nav">
      <button class="navbtn active" data-tab="overview" onclick="showTab('overview')"><span class="ic">&#128202;</span><span>Dashboard</span></button>
      <button class="navbtn" data-tab="electrical" onclick="showTab('electrical')"><span class="ic">&#9889;</span><span>Electrical</span></button>
      <button class="navbtn" data-tab="meters" onclick="showTab('meters')"><span class="ic">&#9648;</span><span>Load &amp; Meters</span></button>
      <button class="navbtn" data-tab="protection" onclick="showTab('protection')"><span class="ic">&#128737;</span><span>Protection</span></button>
      <button class="navbtn" data-tab="energy" onclick="showTab('energy')"><span class="ic">&#128200;</span><span>Energy</span></button>
      <button class="navbtn" data-tab="wifi" onclick="showTab('wifi')"><span class="ic">&#128246;</span><span>Wi-Fi</span></button>
      <button class="navbtn" data-tab="system" onclick="showTab('system')"><span class="ic">&#128187;</span><span>System</span></button>
    </nav>
    <div class="side-foot">
      <div class="conn-w">
        <div class="conn-h"><span class="conn-l">Connection</span><span style="display:flex;align-items:center;gap:6px"><span id="connStateTxt" class="c-warn" style="font-size:.62rem;font-weight:700">Connecting</span><span class="dot" id="connDot" style="color:var(--yellow);box-shadow:0 0 8px currentColor"></span></span></div>
        <p style="font-size:.72rem;font-family:ui-monospace,monospace;color:#fff;font-weight:600" id="connHost">esp32-node</p>
        <div class="bwrap" style="margin-top:10px"><div class="bfill" id="connBar" style="width:20%"></div></div>
        <div style="display:flex;justify-content:space-between;margin-top:7px;font-size:.62rem;color:var(--muted)"><span id="connRssi">Wi-Fi: -- dBm</span><span id="connNet">AP</span></div>
        <div style="margin-top:6px;font-size:.6rem;color:var(--muted)" id="connLast" aria-live="polite">Last update: waiting</div>
        <button class="btn btn-blue" id="retryBtn" onclick="retryPoll()" style="margin-top:10px;padding:8px;font-size:.72rem">&#8635; Retry</button>
      </div>
    </div>
  </aside>
  <div class="main">
    <header class="topbar">
      <div>
        <div class="tb-title" id="tbTitle">System Overview</div>
        <div class="tb-sub">ESP32 Smart Meter ATS Dashboard</div>
      </div>
      <div class="tb-right">
        <div class="tb-clock"><div class="t" id="clkT">--:--:--</div><div class="d" id="clkD">--</div></div>
        <div class="badges">
          <span class="badge bdg-conn off" id="bdgConn" title="PZEM sensor link"><span class="dot"></span><span id="connTxt" class="c-bad">Connecting</span></span>
          <span class="badge bdg-mode" id="bdgMode"><span class="dot"></span><span id="modeTxt">AUTO</span></span>
          <span class="badge bdg-mode" id="bdgWifi" style="display:none"><span class="dot"></span><span id="wifiTxt">WiFi</span></span>
          <span class="badge bdg-conn" id="bdgOta" style="display:none"><span class="dot"></span><span id="otaTxt">OTA Ready</span></span>
          <span class="badge bdg-conn" id="bdgApi" title="Device link — updated on every /api/status poll"><span class="dot" id="apiDot"></span><span id="apiTxt">Link</span>&nbsp;<span id="apiTs" title="Last successful update" style="opacity:.7;font-weight:600">--:--:--</span></span>
        </div>
      </div>
    </header>
    <div class="mtabs">
      <button class="mtab active" data-tab="overview" onclick="showTab('overview')"><span>&#128202;</span>Dashboard</button>
      <button class="mtab" data-tab="electrical" onclick="showTab('electrical')"><span>&#9889;</span>Electrical</button>
      <button class="mtab" data-tab="meters" onclick="showTab('meters')"><span>&#9648;</span>Meters</button>
      <button class="mtab" data-tab="protection" onclick="showTab('protection')"><span>&#128737;</span>Protection</button>
      <button class="mtab" data-tab="energy" onclick="showTab('energy')"><span>&#128200;</span>Energy</button>
      <button class="mtab" data-tab="wifi" onclick="showTab('wifi')"><span>&#128246;</span>Wi-Fi</button>
      <button class="mtab" data-tab="system" onclick="showTab('system')"><span>&#128187;</span>System</button>
    </div>
    <main class="content">
      <div class="hero">
        <div class="hero-row">
          <div>
            <div class="hero-kicker">Smart Meter ATS</div>
            <div class="hero-title">Dashboard2 control surface</div>
            <div class="hero-sub">Live telemetry, protection, energy, Wi-Fi and maintenance in one embedded interface tuned for the ESP32 dashboard.</div>
          </div>
          <div class="hero-chip"><span class="dot" id="heroDot" style="color:var(--red)"></span><span id="heroState">Connecting</span></div>
        </div>
      </div>

      <div class="net-err" id="netErr" role="alert" style="display:none">
        <span>&#9888;</span><span id="netErrMsg">Connection lost — showing last known data.</span>
        <button class="btn btn-blue" onclick="retryPoll()" style="width:auto;margin:0;padding:8px 16px">Retry</button>
      </div>

      <div class="fault-banner" id="faultBanner">
        <div class="fb-icon">&#9888;</div>
        <div class="fb-body">
          <div class="fb-title" id="faultTitle">&#9888; PROTECTION TRIP &mdash; All Relays OFF</div>
          <div class="fb-reason" id="faultReason">Unknown fault</div>
          <div class="fb-reason" id="faultMeta" style="font-size:.9em;opacity:.85;margin-top:2px"></div>
        </div>
        <button class="btn btn-red" style="width:auto;margin-top:0;padding:10px 18px;flex-shrink:0" onclick="clearFault()">&#10003; Clear Fault</button>
      </div>

      <!-- ===== TAB: Dashboard / Overview ===== -->
      <section class="tabpane show" id="tab-overview">
        <div class="metrics">
          <div class="mc v"><div class="mc-hd"><span class="mc-lbl">Line Voltage</span><span class="mc-icon">&#128268;</span></div><div class="mc-val na skel" id="mv">--</div><div class="mc-unit">Volts AC</div></div>
          <div class="mc a"><div class="mc-hd"><span class="mc-lbl">Load Current</span><span class="mc-icon">&#x26A1;</span></div><div class="mc-val na skel" id="ma">--</div><div class="mc-unit">Ampere</div></div>
          <div class="mc w"><div class="mc-hd"><span class="mc-lbl">Active Power</span><span class="mc-icon">&#128161;</span></div><div class="mc-val na skel" id="mw">--</div><div class="mc-unit">Watts</div></div>
          <div class="mc e"><div class="mc-hd"><span class="mc-lbl">Cumulative Energy</span><span class="mc-icon">&#128200;</span></div><div class="mc-val na skel" id="me">--</div><div class="mc-unit">kWh</div></div>
        </div>
        <div class="card soft">
          <div class="ctitle">&#9889; Quick Actions</div>
          <div class="ctrls">
            <div class="cg"><div class="cg-lbl">Refresh Dashboard</div><button class="btn btn-blue" id="qaRefBtn" onclick="qaRefresh()">&#8635; Refresh</button></div>
            <div class="cg"><div class="cg-lbl">Sync RTC</div><button class="btn btn-blue" id="qaSyncBtn" onclick="syncBrowserTime('qaSyncBtn','qaSyncRes')">&#128336; Sync RTC</button><p class="hint" id="qaSyncRes" style="margin-top:6px">RTC: --</p></div>
            <div class="cg"><div class="cg-lbl">Bypass Mode</div><button class="btn btn-amber" id="bypBtn" onclick="toggleBypass()">&#9193; Bypass: OFF</button><p class="hint" style="margin-top:6px">Suspends auto meter switching</p></div>
            <div class="cg"><div class="cg-lbl">Relay / Load Test</div><button class="btn btn-amber tm-toggle" id="qaTmBtn" onclick="toggleTestMode('qaTmBtn')">&#128296; Test Mode: OFF</button><div id="qaTmRelays" class="tm-relays" style="display:none;margin-top:8px"></div></div>
            <div class="cg"><div class="cg-lbl">Emergency</div><button class="btn btn-red" id="emgBtn" onclick="doEmergency()">&#9888; Emergency OFF</button></div>
            <div class="cg"><div class="cg-lbl">Restart ESP32</div><button class="btn btn-amber" id="qaRestartBtn" onclick="doRestart('qaRestartBtn')">&#128260; Restart ESP32</button></div>
            <div class="cg"><div class="cg-lbl">Switch to Meter</div><select class="csel" id="manM"></select><button class="btn btn-green" onclick="manSwitch()">&#8594; Switch Meter</button></div>
            <div class="cg"><div class="cg-lbl">Energy Counter</div><button class="btn btn-amber" onclick="resetEnergy()">&#128259; Reset Energy</button></div>
          </div>
        </div>
        <div class="card soft">
          <div class="ctitle">&#128187; System Snapshot</div>
          <div class="stats-grid" style="grid-template-columns:repeat(2,1fr)">
            <div class="stat-item"><div class="stat-icon">&#128268;</div><div class="stat-val" style="font-size:1rem" id="snapV">--</div><div class="stat-lbl">Voltage</div></div>
            <div class="stat-item"><div class="stat-icon">&#x26A1;</div><div class="stat-val" style="font-size:1rem" id="snapA">--</div><div class="stat-lbl">Current</div></div>
            <div class="stat-item"><div class="stat-icon">&#128246;</div><div class="stat-val" style="font-size:1rem" id="snapW">--</div><div class="stat-lbl">Wi-Fi</div></div>
            <div class="stat-item"><div class="stat-icon">&#128737;</div><div class="stat-val" style="font-size:1rem" id="snapP">--</div><div class="stat-lbl">Protection</div></div>
          </div>
        </div>
      </section>

      <section class="tabpane" id="tab-electrical">
        <div class="grid g2">
          <div class="card">
            <div class="ctitle">&#9889; Live Electrical Parameters</div>
            <div class="stats-grid" style="grid-template-columns:repeat(2,1fr)">
              <div class="stat-item"><div class="stat-icon">&#128268;</div><div class="stat-val" id="elV">--</div><div class="stat-unit">Volts AC</div><div class="stat-lbl">Voltage</div></div>
              <div class="stat-item"><div class="stat-icon">&#x26A1;</div><div class="stat-val" id="elA">--</div><div class="stat-unit">Ampere</div><div class="stat-lbl">Current</div></div>
              <div class="stat-item"><div class="stat-icon">&#128161;</div><div class="stat-val" id="elW">--</div><div class="stat-unit">Watts</div><div class="stat-lbl">Power</div></div>
              <div class="stat-item"><div class="stat-icon">&#128200;</div><div class="stat-val" id="elE">--</div><div class="stat-unit">kWh</div><div class="stat-lbl">Energy</div></div>
            </div>
          </div>
          <div class="card">
            <div class="ctitle">&#128161; Power Flow</div>
            <div style="display:flex;flex-direction:column;gap:14px;padding:8px 2px 4px">
              <div style="display:flex;align-items:center;justify-content:space-between;gap:12px">
                <div class="hero-chip" style="background:rgba(59,130,246,.12)"><span class="dot" style="color:var(--cyan)"></span><span id="pfSource">Grid / PZEM</span></div>
                <div style="flex:1;height:2px;background:linear-gradient(90deg,rgba(59,130,246,.75),rgba(34,197,94,.75));margin:0 8px;border-radius:2px"></div>
                <div class="hero-chip" style="background:rgba(34,197,94,.12)"><span class="dot" style="color:var(--green)"></span><span id="pfLoad">Load Branch</span></div>
              </div>
              <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px">
                <div class="stat-item"><div class="stat-icon">&#128161;</div><div class="stat-val" id="pfPower">--</div><div class="stat-lbl">Active Power</div></div>
                <div class="stat-item"><div class="stat-icon">&#128200;</div><div class="stat-val" id="pfEnergy">--</div><div class="stat-lbl">Cumulative Energy</div></div>
              </div>
            </div>
          </div>
        </div>
      </section>

      <!-- ===== TAB: Load & Meters ===== -->
      <section class="tabpane" id="tab-meters">
        <div class="grid g2">
          <div class="card">
            <div class="ctitle">&#9648; Meter Status</div>
            <div class="meter-list" id="meterList" style="max-height:none"></div>
            <button class="btn btn-blue addmeter" id="addMeterBtn" onclick="addMeter()">&#10133; Add Meter</button>
          </div>
          <div class="card">
            <div class="ctitle">&#9881; Energy Limits</div>
            <div class="limscroll" style="max-height:none"><div id="limitRows"></div></div>
            <button class="btn btn-blue" id="saveLimitsBtn" onclick="saveLimits()">&#128190; Save Limits</button>
          </div>
        </div>
      </section>

      <!-- ===== TAB: Protection & Safety ===== -->
      <section class="tabpane" id="tab-protection">
        <div class="safe-banner" id="safeBanner">
          <div class="sb-l"><span class="sb-ic">&#128737;</span><div><div class="sb-t">System Interlocks Safe &amp; Active</div><div class="sb-s">Continuous monitoring on Voltage (OV/UV) and Current (OC)</div></div></div>
          <span class="sb-tag">LATCH OK</span>
        </div>
        <div class="card">
          <div class="ctitle">&#128737; Protection Thresholds &amp; Recovery</div>
          <div class="prot-grid">
            <div class="pcard"><div class="cg-lbl">Over Voltage Threshold (V)</div><input class="linp" type="number" id="pOV" min="220" max="300" step="1" placeholder="250"></div>
            <div class="pcard"><div class="cg-lbl">Under Voltage Threshold (V)</div><input class="linp" type="number" id="pUV" min="100" max="220" step="1" placeholder="180"></div>
            <div class="pcard"><div class="cg-lbl">Over Current Threshold (A)</div><input class="linp" type="number" id="pOC" min="0.1" max="200" step="0.1" placeholder="16.0"></div>
            <div class="pcard"><div class="cg-lbl">OV Recovery Delay (s)</div><input class="linp" type="number" id="pOVr" min="1" max="300" step="1" placeholder="5"></div>
            <div class="pcard"><div class="cg-lbl">UV Recovery Delay (s)</div><input class="linp" type="number" id="pUVr" min="1" max="300" step="1" placeholder="5"></div>
            <div class="pcard"><div class="cg-lbl">OC Recovery Delay (s)</div><input class="linp" type="number" id="pOCr" min="1" max="300" step="1" placeholder="10"></div>
          </div>
          <div class="ctrls" style="margin-top:14px">
            <button class="btn btn-blue" onclick="saveProtection()">&#128190; Save Thresholds</button>
            <button class="btn btn-amber" onclick="clearFault()">&#9711; Clear Fault</button>
            <button class="btn btn-red" onclick="doEmergency()">&#9888; Trigger Emergency Cutoff</button>
          </div>
        </div>
      </section>

      <!-- ===== TAB: Energy & Scheduler ===== -->
      <section class="tabpane" id="tab-energy">
        <div class="card">
          <div class="ctitle">&#128200; Energy Statistics</div>
          <div class="stats-grid">
            <div class="stat-item"><div class="stat-icon">&#9728;</div><div class="stat-val" id="stToday">--</div><div class="stat-unit">kWh</div><div class="stat-lbl">Today</div></div>
            <div class="stat-item"><div class="stat-icon">&#128197;</div><div class="stat-val" id="stMonth">--</div><div class="stat-unit">kWh</div><div class="stat-lbl">This Month</div></div>
            <div class="stat-item"><div class="stat-icon">&#128336;</div><div class="stat-val" id="stLastMon">--</div><div class="stat-unit">kWh</div><div class="stat-lbl">Last Month</div></div>
            <div class="stat-item"><div class="stat-icon">&#128198;</div><div class="stat-val" id="stNext" style="font-size:1rem">--</div><div class="stat-unit"></div><div class="stat-lbl">Next Reset</div></div>
          </div>
        </div>
        <div class="card">
          <div class="ctitle">&#128202; 30-Day Usage (kWh)</div>
          <div class="graph-wrap" id="graph30"></div>
          <div class="graph-lbl"><span>30 days ago</span><span>Today</span></div>
        </div>
      </section>

      <!-- ===== TAB: Wi-Fi & Network ===== -->
      <section class="tabpane" id="tab-wifi">
        <div class="card">
          <div class="ctitle">&#128246; Current Wi-Fi Status</div>
          <div class="stats-grid" style="grid-template-columns:repeat(auto-fit,minmax(150px,1fr))">
            <div class="stat-item"><div class="stat-icon">&#128225;</div><div class="stat-val" id="wConnState" style="font-size:1rem">--</div><div class="stat-lbl">Connection</div></div>
            <div class="stat-item"><div class="stat-icon">&#128272;</div><div class="stat-val" id="wSsidVal" style="font-size:1rem">--</div><div class="stat-lbl">Connected SSID</div></div>
            <div class="stat-item"><div class="stat-icon">&#128205;</div><div class="stat-val" id="wStaIpVal" style="font-size:1rem">--</div><div class="stat-lbl">Station IP</div></div>
            <div class="stat-item"><div class="stat-icon">&#127760;</div><div class="stat-val" id="wApIpVal" style="font-size:1rem">--</div><div class="stat-lbl">AP / Hotspot IP</div></div>
            <div class="stat-item"><div class="stat-icon">&#9881;</div><div class="stat-val" id="wModeVal" style="font-size:1rem">--</div><div class="stat-lbl">Wi-Fi Mode</div></div>
          </div>
          <p class="hint" id="wStatus" style="margin-top:12px">--</p>
        </div>
        <div class="card">
          <div class="ctitle">&#128246; Network Management</div>
          <div class="ctrls">
            <div class="cg">
              <div class="cg-lbl">Available Networks</div>
              <button class="btn btn-blue" onclick="openScan()">&#128269; Scan &amp; Connect</button>
            </div>
            <div class="cg">
              <div class="cg-lbl">Station</div>
              <button class="btn btn-amber" onclick="doDisconnect()">&#128196; Disconnect</button>
              <p class="hint">Drops the current station link. The hotspot stays up and the dashboard remains reachable.</p>
            </div>
            <div class="cg">
              <div class="cg-lbl">Saved Credentials</div>
              <button class="btn btn-red" onclick="doForget()">&#128465; Forget Network</button>
              <p class="hint">Erases the saved SSID/password from NVS and falls back to AP-only mode.</p>
            </div>
          </div>
        </div>
        <div class="card">
          <div class="ctitle">&#9881; Wi-Fi Mode</div>
          <div class="ctrls">
            <div class="cg">
              <div class="cg-lbl">Operation Mode</div>
              <select class="csel" id="wMode" onchange="saveMode()">
                <option value="0">AP Only (hotspot)</option>
                <option value="1">Station Only (home WiFi)</option>
                <option value="2">AP + Station</option>
              </select>
              <p class="hint" id="wModeHint" style="margin-top:10px">AP Only = the ESP32 runs its own hotspot only. Clients connect straight to &ldquo;SmartMeterATS&rdquo;.</p>
            </div>
          </div>
          <div class="ctrls" style="margin-top:14px">
            <div class="cg"><div class="cg-lbl">AP Only</div><p class="hint">ESP32 hotspot only &mdash; no external network. Devices join &ldquo;SmartMeterATS&rdquo; directly.</p></div>
            <div class="cg"><div class="cg-lbl">Station Only</div><p class="hint">Connects to an existing Wi-Fi network only. The hotspot stops once the link is proven, and returns automatically if the link drops.</p></div>
            <div class="cg"><div class="cg-lbl">AP + Station</div><p class="hint">Hotspot and external Wi-Fi run simultaneously &mdash; dashboard reachable on both IPs.</p></div>
          </div>
        </div>
      </section>

      <!-- ===== TAB: System Health & Maintenance ===== -->
      <section class="tabpane" id="tab-system">
        <div class="card">
          <div class="ctitle">&#128187; System Information</div>
          <div class="stats-grid" id="sysGrid" style="grid-template-columns:repeat(auto-fit,minmax(150px,1fr))"></div>
        </div>
        <div class="card">
          <div class="ctitle">&#128295; Maintenance</div>
          <div class="ctrls">
            <div class="cg">
              <div class="cg-lbl">Relay Test Mode</div>
              <button class="btn btn-amber tm-toggle" id="tmBtn" onclick="toggleTestMode('tmBtn')">&#128296; Test Mode: OFF</button>
              <div id="tmRelays" class="tm-relays" style="display:none;margin-top:8px"></div>
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
              <button class="btn btn-blue" id="otaBtn" onclick="document.getElementById('fwFile').click()">&#128190; Upload .bin</button>
              <div class="bwrap" style="margin-top:8px"><div class="bfill" id="otaBar" style="width:0%"></div></div>
              <p class="hint" id="otaMsg">--</p>
            </div>
            <div class="cg">
              <div class="cg-lbl">Restart</div>
              <button class="btn btn-amber" onclick="doRestart()">&#128260; Restart ESP32</button>
              <p class="hint">Reboots the device in about 1&ndash;2 seconds. Settings and NVS data are preserved.</p>
            </div>
            <div class="cg">
              <div class="cg-lbl">Danger Zone</div>
              <button class="btn btn-red" onclick="doFactoryReset()">&#9888; Factory Reset</button>
              <p class="hint">Erases ALL settings and reboots with defaults.</p>
            </div>
          </div>
        </div>
        <div class="card">
          <div class="ctitle">&#128220; Reports &amp; Export</div>
          <div class="ctrls">
            <div class="cg">
              <div class="cg-lbl">Event Log CSV</div>
              <button class="btn btn-blue" id="expEvBtn" onclick="exportEventsCSV('expEvBtn')">&#11015; Export Event Log</button>
              <p class="hint">Date, time, type, description, meter/relay and value for every logged event.</p>
            </div>
            <div class="cg">
              <div class="cg-lbl">Energy &amp; Meter CSV</div>
              <button class="btn btn-blue" id="expEnBtn" onclick="exportEnergyCSV('expEnBtn')">&#11015; Export Energy</button>
              <select class="csel" id="expRange" onchange="expRangeUI()" style="margin-top:8px" aria-label="Energy report range">
                <option value="today">Today</option>
                <option value="7d">Last 7 Days</option>
                <option value="30d" selected>Last 30 Days</option>
                <option value="custom">Custom range</option>
              </select>
              <div id="expCustomRange" style="display:none;margin-top:8px">
                <input class="linp" type="date" id="expFrom" aria-label="From date">
                <input class="linp" type="date" id="expTo" aria-label="To date" style="margin-top:6px">
              </div>
              <p class="hint">Live snapshot, period totals, per-meter usage and the daily history the firmware already keeps.</p>
            </div>
          </div>
        </div>
        <div class="card">
          <div class="ctitle">&#128203; Event Log <button class="btn btn-blue" id="expEvHdrBtn" style="width:auto;float:right;margin:-6px 6px 0 0;padding:5px 12px" onclick="exportEventsCSV('expEvHdrBtn')">&#11015; Export</button> <button class="btn btn-amber" style="width:auto;float:right;margin:-6px 0 0;padding:5px 12px" onclick="clearEvents()">Clear</button></div>
          <div class="ev-toolbar">
            <div class="ev-filters" id="evFilters">
              <button class="evf active" data-cat="all" onclick="evFilter('all')">All</button>
              <button class="evf" data-cat="info" onclick="evFilter('info')">Info</button>
              <button class="evf" data-cat="warn" onclick="evFilter('warn')">Warning</button>
              <button class="evf" data-cat="fault" onclick="evFilter('fault')">Fault</button>
              <button class="evf" data-cat="sec" onclick="evFilter('sec')">Security</button>
              <button class="evf" data-cat="sys" onclick="evFilter('sys')">System</button>
            </div>
            <input class="linp ev-search" type="search" id="evSearch" placeholder="Search event, meter, value..." aria-label="Search events" oninput="evRender()">
          </div>
          <div class="ev-err" id="evErr" role="alert" style="display:none"><span>Failed to load event log.</span><button class="btn btn-blue" onclick="evRetry()">&#8635; Retry</button></div>
          <div id="evList" class="ev-list"></div>
        </div>
        <div class="footer" id="accessInfo">Access: http://192.168.4.1/</div>
      </section>
    </main>
  </div>
</div>

<div id="toast" role="status" aria-live="polite"></div>
<div class="wmodal" id="lgModal" role="dialog" aria-modal="true" aria-labelledby="lgTitle" onclick="if(event.target===this)hideLogin()">
  <div class="wmbox" style="max-width:340px">
    <div class="wmhdr"><span class="wmtitle" id="lgTitle">&#128274; Device Login</span></div>
    <p class="hint" style="margin:0 0 10px">Enter the device password to enable emergency stop, config import, factory reset and firmware upload.</p>
    <input class="linp" type="password" id="lgPw" maxlength="64" placeholder="Device password" onkeydown="if(event.key==='Enter')doLogin()">
    <button class="btn btn-blue" onclick="doLogin()">&#10003; Unlock</button>
    <p class="wmerr" id="lgErr" aria-live="polite"></p>
  </div>
</div>
<div class="wmodal" id="wModal" onclick="if(event.target===this)closeScan()">
  <div class="wmbox">
    <div class="wmhdr">
      <span class="wmtitle">Available Networks</span>
      <button class="wmbtn" onclick="scanNets()" title="Refresh" aria-label="Refresh scan">&#8635;</button>
      <button class="wmbtn" onclick="closeScan()" title="Close" aria-label="Close">&#10005;</button>
    </div>
    <p class="hint" id="wScanMsg" style="margin:0 0 8px">&#128270; Scanning...</p>
    <div id="wList"></div>
    <p class="wmerr" id="wErr" aria-live="polite"></p>
  </div>
</div>
<div class="wmodal" id="cfModal" role="dialog" aria-modal="true" aria-labelledby="cfTitle" onclick="if(event.target===this)closeConfirm()">
  <div class="wmbox" style="max-width:380px">
    <div class="wmhdr"><span class="wmtitle" id="cfTitle">Confirm</span></div>
    <p class="hint" id="cfBody" style="margin:0 0 14px;font-size:.82rem;line-height:1.6;color:var(--text)"></p>
    <div style="display:flex;gap:10px">
      <button class="btn btn-red" id="cfOk" style="flex:1;margin-top:0"></button>
      <button class="btn btn-amber" onclick="closeConfirm()" style="flex:1;margin-top:0">Cancel</button>
    </div>
    <p class="wmerr" id="cfErr"></p>
  </div>
</div>
<script>
// N = Active Meters reported by the firmware. Every meter card,
// limit row and dropdown option below is generated from N —
// nothing meter-specific is hard-coded in this page.
var N=0;
var _tt;
// One confirm wrapper for every destructive action, so the dialog text and
// the yes/no gate live in exactly one place (see confirmDo/closeConfirm above).
// SSID the station is currently linked to ('' while none). Refreshed by
// updBadges() and used by the scan modal to badge the connected network.
var _curSsid='';
// ===== Dashboard2 shell: tab navigation + header clock =====
var TAB_TITLES={overview:'System Overview',electrical:'Live Electrical Parameters',meters:'Load & Meter Management',protection:'Safety & Protection Matrix',energy:'Energy Analytics & Scheduler',wifi:'Wi-Fi & Network Settings',system:'System Health & Maintenance'};
var _curTab='overview';
function showTab(id){
  _curTab=id;
  var panes=document.querySelectorAll('.tabpane');
  for(var i=0;i<panes.length;i++)panes[i].className='tabpane'+(panes[i].id==='tab-'+id?' show':'');
  var btns=document.querySelectorAll('.navbtn,.mtab');
  for(var j=0;j<btns.length;j++){var a=btns[j].getAttribute('data-tab')===id;btns[j].className=btns[j].className.replace(' active','')+(a?' active':'');}
  document.getElementById('tbTitle').textContent=TAB_TITLES[id]||'System Overview';
  // Event log lives only in the System tab — refresh it the moment that tab is
  // shown, instead of polling it constantly in the background.
  if(id==='system')loadEvents(true);
}
function tickClock(){
  var n=new Date();
  var p=function(x){return(x<10?'0':'')+x;};
  document.getElementById('clkT').textContent=p(n.getHours())+':'+p(n.getMinutes())+':'+p(n.getSeconds());
  document.getElementById('clkD').textContent=n.toLocaleDateString('en-US',{month:'short',day:'numeric',year:'numeric'});
}
// One styled confirm dialog for every destructive action — replaces native
// confirm() so dangerous operations can't be dismissed by an accidental
// Enter, and so the message styling matches the dashboard. Action callbacks
// are queued by index and only run when "Confirm" is pressed.
var _confirmCb=null;
function confirmDo(title,body,btnLabel,cb){
  document.getElementById('cfTitle').textContent=title;
  document.getElementById('cfBody').textContent=body;
  document.getElementById('cfOk').textContent=btnLabel||'Confirm';
  _confirmCb=cb;
  document.getElementById('cfModal').className='wmodal show';
  document.getElementById('cfOk').focus();
}
function closeConfirm(){
  document.getElementById('cfModal').className='wmodal';
  _confirmCb=null;
}
function confirmOk(){
  var cb=_confirmCb;
  closeConfirm();
  if(cb)cb();
}
// Connectivity state machine. apiOnline is set from poll() success/failure;
// the header badge + hero chip + sidebar dot all derive from it so a dropped
// /api/status is visible within one poll cycle even if the PZEM still reports OK.
var apiOnline=null,lastApiOkTs=0,apiFails=0;
function setApiOnline(on){
  if(on){apiOnline=true;lastApiOkTs=Date.now();apiFails=0;}
  else{if(apiOnline!==false){apiOnline=false;} }
  var badge=document.getElementById('bdgApi');
  var apiDot=document.getElementById('apiDot');
  var apiTxt=document.getElementById('apiTxt');
  var ts=document.getElementById('apiTs');
  var side=document.getElementById('connStateTxt');
  var dot=document.getElementById('connDot');
  var hero=document.getElementById('heroDot');
  var heroTxt=document.getElementById('heroState');
  if(apiOnline===true){
    if(badge)badge.className='badge bdg-conn';
    if(apiDot)apiDot.style.color='var(--green)';
    if(apiTxt){apiTxt.textContent='Online';apiTxt.className='c-ok';}
    if(side){side.textContent='Online';side.className='c-ok';}
    if(ts)ts.textContent=new Date(lastApiOkTs).toLocaleTimeString();
    if(dot)dot.style.color='var(--green)';
    if(hero)hero.style.color='var(--green)';
    if(heroTxt&&heroTxt.textContent==='Connection Lost')heroTxt.textContent='Connected';
    var cl=document.getElementById('connLast');
    if(cl)cl.textContent='Updated '+new Date(lastApiOkTs).toLocaleTimeString();
  }else if(apiOnline===false){
    if(badge)badge.className='badge bdg-conn off';
    if(apiDot)apiDot.style.color='var(--red)';
    if(apiTxt){apiTxt.textContent='Offline';apiTxt.className='c-bad';}
    if(side){side.textContent='Offline';side.className='c-bad';}
    if(ts)ts.textContent='--:--:--';
    if(dot)dot.style.color='var(--red)';
    if(hero)hero.style.color='var(--red)';
    if(heroTxt)heroTxt.textContent='Connection Lost';
    var cl=document.getElementById('connLast');
    if(cl)cl.textContent='Last update: lost';
  }
}
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
// fetch for a protected route: attaches Basic auth, and on a 401 clears the
// bad credential, re-prompts, and retries once with the new password. Every
// mutating/secret call goes through this so no call site handles auth itself.
async function apiFetch(url,opts){
  opts=opts||{};
  opts.headers=Object.assign({},opts.headers,await authHdr());
  var r=await fetch(url,opts);
  if(r.status===401){                    // cached password rejected
    _auth=null;
    await showLogin('Wrong password — try again');
    if(_auth===null)return r;            // cancelled — caller sees 401 JSON body
    opts.headers=Object.assign({},opts.headers,await authHdr());
    r=await fetch(url,opts);
  }
  return r;
}
// wait — `document.write` fires before <body> exists, so defer to load
window.addEventListener('load',function(){document.body.classList.add('loaded');});
function timeNow(){
  var n=new Date(),p=function(x){return(x<10?'0':'')+x;};
  return p(n.getHours())+':'+p(n.getMinutes())+':'+p(n.getSeconds());
}
function toast(m,t){var e=document.getElementById('toast');if(!e)return;e.textContent=m;e.className='show '+(t||'ok');clearTimeout(_tt);_tt=setTimeout(function(){e.textContent='';e.className=''},3200);}
// ===== Async-op UX helpers =====
// Disables a button + swaps its label while an operation runs, and returns
// false if the button is already busy (blocks duplicate requests). Restore
// with btnIdle(). Only ever wired to one button id per operation.
var _busyBtn={};
function btnBusy(id,label){
  var b=document.getElementById(id);
  if(!b||_busyBtn[id])return false;
  _busyBtn[id]=true;
  b.dataset.txt=b.innerHTML;
  b.innerHTML=label||'Working...';
  b.disabled=true;
  return true;
}
function btnIdle(id){
  var b=document.getElementById(id);
  if(!b)return;
  _busyBtn[id]=false;
  b.innerHTML=b.dataset.txt||b.innerHTML;
  b.disabled=false;
}
// One mutating-call helper: auth + 20s network timeout + friendly failure
// toast. Mirrors apiFetch (401 re-prompt + retry) but times only the actual
// fetch, never the login modal. Returns the parsed JSON body, or
// {status:'error',msg:'network'|'timeout'|'parse'|'auth'} on failure — the
// transport cases already toast, so callers only show d.msg for real backend
// errors.
async function apiCall(url,opts){
  opts=opts||{};
  opts.headers=Object.assign({},opts.headers,await authHdr());
  var t;
  try{
    var r=await Promise.race([
      fetch(url,opts),
      new Promise(function(_,rej){t=setTimeout(function(){rej(new Error('timeout'));},20000);})
    ]);
    if(r.status===401){
      _auth=null;
      await showLogin('Wrong password — try again');
      if(_auth===null)return {status:'error',msg:'auth'};
      opts.headers=Object.assign({},opts.headers,await authHdr());
      r=await fetch(url,opts);
    }
    return await r.json();
  }catch(e){
    var to=e&&e.message==='timeout';
    toast(to?'Request timed out — device busy':'Connection failed — check connection','err');
    return {status:'error',msg:to?'timeout':'network'};
  }finally{clearTimeout(t);}
}
function isNetErr(d){return d&&d.status==='error';}
// ===== The one time authFetch/apiFetch retry w/ backoff for the mutating calls =====
// (ponytail: one wrapper, not a whole retry framework; only used where a hiccup
//  looks like auth and a blind re-fetch is cheap.)
async function authRetry(url,opts){
  var r=await apiFetch(url,opts);
  if(r.status===401){           // apiFetch already handled one retry; still 401
    // user-visible and non-crashing — bail with the original response
    return r;
  }
  return r;
}
// Escape closes whichever modal is on top; Enter confirms the styled dialog.
document.addEventListener('keydown',function(e){
  if(e.key==='Escape'){
    var cf=document.getElementById('cfModal');
    var lg=document.getElementById('lgModal');
    var wm=document.getElementById('wModal');
    if(cf&&cf.className.indexOf('show')>=0)closeConfirm();
    else if(lg&&lg.className.indexOf('show')>=0)hideLogin();
    else if(wm&&wm.className.indexOf('show')>=0)closeScan();
  }else if(e.key==='Enter'){
    var cf=document.getElementById('cfModal');
    if(cf&&cf.className.indexOf('show')>=0&&_confirmCb){confirmOk();}
  }
});
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
      +'<div class="mrm" id="rm'+i+'" onclick="removeMeter('+i+')" title="Remove Meter '+(i+1)+'" aria-label="Remove Meter '+(i+1)+'">&times;</div>'
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
  ct.textContent=d.pzemOK?'🟢 Connected':'🔴 Disconnected';
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
    wt.textContent=d.staOK?('WiFi: Connected'):(d.staFallback?'WiFi: Reconnecting':'WiFi: Connecting');
    bw.title=d.staOK?((d.staSsid||'')+(d.staRssi?' • '+d.staRssi+' dBm':'')):(d.staSsid||'');
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
      :(d.staFallback?'Reconnecting to '+(d.staSsid||'?')+'...':'Connecting to '+(d.staSsid||'?')+'...'));
    var wm=document.getElementById('wMode');
    if(!wm.dataset.loaded){wm.value=d.wifiMode||0;wm.dataset.loaded='1';}
  }
  // Track the current SSID so the scan modal can badge the connected row.
  _curSsid=d.staOK?(d.staSsid||''):'';
  // WiFi tab status cards — refreshed on the /api/status poll, so they
  // reflect the live link without a second endpoint.
  var st=document.getElementById('wConnState');
  if(st){
    if(d.wifiMode===0){st.textContent='Hotspot Only';st.style.color='var(--yellow)';}
    else if(d.staOK){st.textContent='Connected';st.style.color='var(--green)';}
    else if(d.staFallback){st.textContent='Reconnecting...';st.style.color='var(--yellow)';}
    else{st.textContent='Connecting...';st.style.color='var(--yellow)';}
  }
  var sv=document.getElementById('wSsidVal');if(sv)sv.textContent=d.staOK?(d.staSsid||'--'):'--';
  var ip=document.getElementById('wStaIpVal');if(ip)ip.textContent=d.staOK?(d.staIP||'--'):'--';
  var ap=document.getElementById('wApIpVal');if(ap)ap.textContent=d.apIP||'--';
  var mv=document.getElementById('wModeVal');
  if(mv)mv.textContent=d.wifiMode===0?'AP Only':(d.wifiMode===1?'Station Only':'AP + Station');
  // Dashboard2 sidebar connection widget
  var cd=document.getElementById('connDot');
  if(cd)cd.style.color=d.pzemOK?'var(--green)':'var(--red)';
  var cb=document.getElementById('connBar');
  if(cb){var q=d.staRssi?Math.min(100,Math.max(15,(d.staRssi+100)*2)):(d.pzemOK?70:20);cb.style.width=q+'%';cb.className='bfill'+(q<40?' danger':q<60?' warn':'');}
  var cr=document.getElementById('connRssi');if(cr)cr.textContent='Wi-Fi: '+(d.staRssi?d.staRssi+' dBm':'--');
  var cn=document.getElementById('connNet');if(cn)cn.textContent=(d.wifiMode>0&&d.staOK)?(d.staSsid||'STA'):'AP Mode';
  // Protection tab safe/latch banner (hidden during a trip; fault-banner covers that)
  var sb=document.getElementById('safeBanner');
  if(sb)sb.style.display=(d.protTrip||d.emergency)?'none':'flex';
  var hs=document.getElementById('heroState');
  if(hs)hs.textContent=d.protTrip?'Protection Fault':(d.emergency?'Emergency Off':(d.bypass?'Bypass Active':'Normal Operation'));
  var hd=document.getElementById('heroDot');
  if(hd)hd.style.color=d.protTrip||d.emergency?'var(--red)':(d.bypass?'var(--yellow)':'var(--green)');
  var sv=document.getElementById('snapV');if(sv)sv.textContent=d.voltage?d.voltage.toFixed(1)+' V':'--';
  var sa=document.getElementById('snapA');if(sa)sa.textContent=d.current?d.current.toFixed(2)+' A':'--';
  var sw=document.getElementById('snapW');if(sw)sw.textContent=d.staOK?('Wi-Fi '+(d.staRssi?d.staRssi+' dBm':'OK')):(d.wifiMode===0?'AP Mode':'Wi-Fi Offline');
  var sp=document.getElementById('snapP');if(sp)sp.textContent=d.protTrip?'Fault':(d.emergency?'Safe Off':'OK');
  var elV=document.getElementById('elV');if(elV)elV.textContent=d.voltage?d.voltage.toFixed(1):'--';
  var elA=document.getElementById('elA');if(elA)elA.textContent=d.current?d.current.toFixed(2):'--';
  var elW=document.getElementById('elW');if(elW)elW.textContent=d.power?d.power.toFixed(1):'--';
  var elE=document.getElementById('elE');if(elE)elE.textContent=d.energy?d.energy.toFixed(3):'--';
  var pfPower=document.getElementById('pfPower');if(pfPower)pfPower.textContent=d.power?d.power.toFixed(1)+' W':'--';
  var pfEnergy=document.getElementById('pfEnergy');if(pfEnergy)pfEnergy.textContent=d.energy?d.energy.toFixed(3)+' kWh':'--';
  var pfSource=document.getElementById('pfSource');if(pfSource)pfSource.textContent=d.pzemOK?(d.staOK?'Grid / PZEM':'PZEM Sensor'):'Sensor Offline';
  var pfLoad=document.getElementById('pfLoad');if(pfLoad)pfLoad.textContent='Load -> Meter '+((d.activeMeter!=null)?(d.activeMeter+1):'?');
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
function togglePw(i){
  var inp=document.getElementById('pw'+i),btn=document.getElementById('pwTgl'+i);
  var show=inp.type==='password';
  inp.type=show?'text':'password';
  btn.innerHTML=show?'&#128065;':'&#128064;';
}
async function scanNets(){
  wErr('');
  var msg=document.getElementById('wScanMsg');
  document.getElementById('wList').innerHTML='<div class="wspin"></div>';
  try{
    // Scan is unauthenticated — a plain fetch so no login prompt appears
    // just to list networks.
    var r=await Promise.race([
      fetch('/api/scanWiFi'),
      new Promise(function(_,rej){setTimeout(function(){rej(new Error('timeout'));},15000);})
    ]);
    if(!r.ok)throw new Error('HTTP '+r.status);
    var d=await r.json();
    var n=d.networks||[];
    if(msg)msg.textContent='';
    if(!n.length){document.getElementById('wList').innerHTML='<p class="hint" style="padding:12px">No networks found.</p>';return;}
    var h='';
    for(var i=0;i<n.length;i++){
      var s=n[i],e=s.encrypted;
      var connected=_curSsid&&s.ssid&&s.ssid.toLowerCase()===_curSsid.toLowerCase();
      h+='<div class="nrow">'+bars(s.rssi)+'<span>'+(e?'&#128274;':'&#128275;')+'</span>'
        +'<span class="nssid" id="ns'+i+'"></span>';
      if(connected)h+='<span class="badge bdg-conn" style="flex-shrink:0;white-space:nowrap">&#10003; Connected</span>';
      h+='<button class="btn btn-blue" style="width:auto;margin:0;padding:6px 14px" id="nb'+i+'" onclick="pick('+i+','+(e?1:0)+')" '+(connected?'disabled':'')+'>'+(connected?'Connected':'Connect')+'</button></div>'
        +'<div class="npwrap" id="np'+i+'"><div style="display:flex;gap:8px;margin:0 12px 10px"><input class="linp" type="password" maxlength="64" id="pw'+i+'" placeholder="Password"><button class="wmbtn" id="pwTgl'+i+'" onclick="togglePw('+i+')" title="Show/Hide Password" style="width:38px;height:38px;flex-shrink:0">&#128065;</button></div></div>';
    }
    document.getElementById('wList').innerHTML=h;
    // textContent, never innerHTML — an SSID may contain markup
    for(var j=0;j<n.length;j++)document.getElementById('ns'+j).textContent=n[j].ssid;
    _nets=n;
  }catch(ex){document.getElementById('wList').innerHTML='';if(msg)msg.textContent='';wErr('Scan failed — try again.');}
}
var _nets=[],_busyConn=false;
function pick(i,enc){
  var b=document.getElementById('nb'+i),w=document.getElementById('np'+i);
  if(!enc)return doConnect(i,'');
  if(w.className.indexOf('open')<0){w.className='npwrap open';b.textContent='Confirm';document.getElementById('pw'+i).focus();return;}
  doConnect(i,document.getElementById('pw'+i).value);
}
async function doConnect(i,pass){
  if(_busyConn)return;                 // one association attempt at a time
  var b=document.getElementById('nb'+i);
  wErr('');_busyConn=true;b.textContent='Connecting...';b.disabled=true;
  try{
    var m=document.getElementById('wMode').value;
    if(m==='0')m='2';   // AP-only can't hold a station link — go AP+STA
    var d=await apiCall('/api/connectWiFi?mode='+m+'&ssid='+encodeURIComponent(_nets[i].ssid)
      +'&pass='+encodeURIComponent(pass));
    if(d.status==='error'){_busyConn=false;b.textContent='Connect';b.disabled=false;return;}
    if(d.status!=='ok'){wErr(d.msg||'Connection failed');_busyConn=false;b.textContent='Connect';b.disabled=false;return;}
    // Association runs in the background; poll for the verdict. A fresh
    // poll of the wifi tab keeps the new SSID visible while we wait.
    for(var t=0;t<32;t++){
      await new Promise(function(f){setTimeout(f,1000);});
      var s=await(await fetch('/api/wifiStatus')).json();
      if(s.staOK){
        _curSsid=s.ssid||'';
        toast('Connected to '+s.ssid+' — '+s.staIP,'ok');
        closeScan();poll();
        _busyConn=false;
        return;
      }
      if(s.err){wErr(s.err);_busyConn=false;b.textContent='Connect';b.disabled=false;return;}
    }
    wErr('Connection timeout');_busyConn=false;b.textContent='Connect';b.disabled=false;
  }catch(ex){wErr('Connection failed');_busyConn=false;b.textContent='Connect';b.disabled=false;}
}
async function saveMode(){
  var m=document.getElementById('wMode').value;
  var hints=['AP Only = the ESP32 runs its own hotspot only. Clients connect straight to &ldquo;SmartMeterATS&rdquo;.','Station Only = connect to an existing Wi-Fi network only. The hotspot stops once the link is proven, and returns automatically if the link drops.','AP + Station = hotspot and external Wi-Fi run simultaneously &mdash; dashboard reachable on both IPs.'];
  var hh=document.getElementById('wModeHint');if(hh)hh.innerHTML=hints[m]||'';
  var d=await apiCall('/api/setWiFi?mode='+m);
  if(d.status==='ok')toast('WiFi mode saved — applying...','ok');
  else if(!isNetErr(d))toast(d.msg||'Error saving WiFi mode','err');
}
function doDisconnect(){
  confirmDo('Disconnect Wi-Fi?',
    'The station link to "'+(_curSsid||'current network')+'" will be dropped. The hotspot stays up and the dashboard remains reachable.',
    'Disconnect',async function(){
      var d=await apiCall('/api/disconnectWiFi');
      if(d.status==='ok')toast('Station disconnected','warn');
      else if(!isNetErr(d))toast(d.msg||'Disconnect failed','err');
      _curSsid='';poll();
    });
}
function doForget(){
  confirmDo('Forget this network?',
    'The saved SSID and password will be erased from NVS and the device falls back to AP-only mode. You will need to re-enter the password to connect again.',
    'Forget Network',async function(){
      var d=await apiCall('/api/forgetWiFi');
      if(d.status==='ok')toast('Network forgotten — AP only','warn');
      else if(!isNetErr(d))toast(d.msg||'Forget failed','err');
      _curSsid='';poll();
    });
}
function updProtection(d){
  var fb=document.getElementById('faultBanner');
  var fr=document.getElementById('faultReason');
  var ft=document.getElementById('faultTitle');
  var fm=document.getElementById('faultMeta');
  if(d.protTrip){
    fb.className='fault-banner show';
    fr.textContent=(d.faultName?d.faultName+' — ':'')+(d.protReason||'Unknown fault');
    var lf=d.lastFaultEpoch?new Date(d.lastFaultEpoch*1000).toLocaleString():'—';
    if(d.faultStatus==='recovering'){
      ft.innerHTML='&#8635; RECOVERING &mdash; stabilizing before restore';
      fm.textContent='Auto-recovery in '+(d.recoverCountdown||0)+'s (stable window '+(d.recoverDelay||0)+'s) · Last fault: '+lf;
    }else{
      ft.innerHTML='&#9888; PROTECTION TRIP &mdash; All Relays OFF';
      fm.textContent='Status: Active · Waiting for stable conditions · Last fault: '+lf;
    }
  }else{fb.className='fault-banner';fm.textContent='';}
  var ove=document.getElementById('pOV');
  var uve=document.getElementById('pUV');
  var oce=document.getElementById('pOC');
  if(d.ovVolt&&!ove.dataset.loaded){ove.value=d.ovVolt;ove.dataset.loaded='1';}
  if(d.uvVolt&&!uve.dataset.loaded){uve.value=d.uvVolt;uve.dataset.loaded='1';}
  if(d.ocCurr&&!oce.dataset.loaded){oce.value=d.ocCurr;oce.dataset.loaded='1';}
  var ovr=document.getElementById('pOVr');
  var uvr=document.getElementById('pUVr');
  var ocr=document.getElementById('pOCr');
  if(d.ovRec&&!ovr.dataset.loaded){ovr.value=d.ovRec;ovr.dataset.loaded='1';}
  if(d.uvRec&&!uvr.dataset.loaded){uvr.value=d.uvRec;uvr.dataset.loaded='1';}
  if(d.ocRec&&!ocr.dataset.loaded){ocr.value=d.ocRec;ocr.dataset.loaded='1';}
}
async function saveProtection(){
  var ov=parseFloat(document.getElementById('pOV').value)||250;
  var uv=parseFloat(document.getElementById('pUV').value)||180;
  var oc=parseFloat(document.getElementById('pOC').value)||16;
  var ovr=Math.round((parseFloat(document.getElementById('pOVr').value)||5)*1000);
  var uvr=Math.round((parseFloat(document.getElementById('pUVr').value)||5)*1000);
  var ocr=Math.round((parseFloat(document.getElementById('pOCr').value)||10)*1000);
  var d=await apiCall('/api/setProtection?ov='+ov+'&uv='+uv+'&oc='+oc+'&ovrec='+ovr+'&uvrec='+uvr+'&ocrec='+ocr);
  if(d.status==='ok')toast('Protection settings saved!','ok');
  else if(!isNetErr(d))toast(d.msg||'Error saving settings','err');
}
async function clearFault(){
  var d=await apiCall('/api/clearFault');
  if(d.status==='ok')toast('Fault cleared — relay restored','ok');
  else if(!isNetErr(d))toast(d.msg||'Error clearing fault','err');
}
function updStats(d){
  if(!d)return;
  var mn=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  document.getElementById('stToday').textContent=(d.todayUsed!=null)?d.todayUsed.toFixed(3):'--';
  document.getElementById('stMonth').textContent=(d.thisMonth!=null)?d.thisMonth.toFixed(3):'--';
  document.getElementById('stLastMon').textContent=(d.lastMonth!=null)?d.lastMonth.toFixed(3):'--';
  var now=(d.rtcYear&&d.rtcMonth)?new Date(d.rtcYear,d.rtcMonth-1,d.rtcDay||1):new Date();
  var rd=d.resetDay||1;
  var nr=(now.getDate()<rd)?new Date(now.getFullYear(),now.getMonth(),rd):new Date(now.getFullYear(),now.getMonth()+1,rd);
  document.getElementById('stNext').textContent=mn[nr.getMonth()]+' '+rd+', '+nr.getFullYear();
  var daily=d.daily&&d.daily.length?d.daily:null;
  var g=document.getElementById('graph30');
  if(daily&&(!g.dataset.built || g.children.length!==daily.length)){
    g.innerHTML='';
    var mx=Math.max.apply(null,daily)||1;
    for(var i=0;i<daily.length;i++){
      var b=document.createElement('div');
      b.className='gb'+(i===daily.length-1?' today':'');
      b.style.height=Math.max(2,Math.round(daily[i]/mx*80))+'px';
      b.setAttribute('data-v',daily[i].toFixed(3)+' kWh');
      g.appendChild(b);
    }
    g.dataset.built='1';
  }else if(daily){
    var mx=Math.max.apply(null,daily)||1;
    for(var j=0;j<daily.length;j++){
      var bar=g.children[j];
      if(!bar)continue;
      bar.className='gb'+(j===daily.length-1?' today':'');
      bar.style.height=Math.max(2,Math.round(daily[j]/mx*80))+'px';
      bar.setAttribute('data-v',daily[j].toFixed(3)+' kWh');
    }
  }
}
// Overlap guard: poll() may be called by the interval, retryPoll(), and the
// Quick Actions "Refresh" button. A fetch already in flight is dropped so a
// manual refresh and the periodic poll never race and duplicate the request.
var _pollInFlight=false;
async function poll(){
  if(_pollInFlight)return;
  _pollInFlight=true;
  var r;
  try{
    r=await fetch('/api/status');
    if(!r.ok)throw new Error('HTTP '+r.status);
    var d=await r.json();
    apiFails=0;
    setApiOnline(true);
    var nb=document.getElementById('netErr');
    if(nb&&nb.style.display==='flex')nb.style.display='none';
    if(document.hidden){return;}      // background tab: keep link, skip DOM churn
    if(d.meterCount!==N)buildUI(d.meterCount);
    var ab=document.getElementById('addMeterBtn');
    if(ab&&!_busyBtn['addMeterBtn'])ab.disabled=(d.meterCount>=(d.maxMeters||10));
    var na=!d.pzemOK;
    setVal('mv',d.voltage,1,na);
    setVal('ma',d.current,2,na);
    setVal('mw',d.power,1,na);
    setVal('me',d.energy,3,na);
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
  }catch(ex){
    // Connection error — keep the last good values on screen (never blank
    // them), mark the device offline, and surface a non-blocking notice.
    apiFails++;
    setApiOnline(false);
    if(apiFails===1||apiFails%10===0)toast('Connection lost — keeping last data. Retrying…','err');
    if(apiFails===1){ // expose the stale-data notice + manual retry once
      var nb=document.getElementById('netErr');
      if(nb){nb.style.display='flex';}
    }
  }finally{
    _pollInFlight=false;
  }
}
// Quick Action: refresh only the live /api/status payload (the single source
// poll() renders the whole dashboard from) and skip any poll already running.
async function qaRefresh(){
  var b=document.getElementById('qaRefBtn');
  if(!b)return;
  b.innerHTML='&#8635; Refreshing...';
  b.disabled=true;
  await poll();
  b.innerHTML='&#8635; Refresh';
  b.disabled=false;
  if(apiOnline===false)toast('Refresh failed — device unreachable','err');
}
async function retryPoll(){
  var nb=document.getElementById('netErr');
  if(nb)nb.style.display='none';
  toast('Retrying…','warn');
  await poll();
  if(apiOnline===true&&nb)nb.style.display='none';
}
async function saveLimits(){
  if(!btnBusy('saveLimitsBtn','Saving...'))return;
  var q='';
  for(var i=0;i<N;i++){
    var v=parseFloat(document.getElementById('lim'+i).value)||5;
    q+=(q?'&':'?')+'l'+i+'='+v;
  }
  var d=await apiCall('/api/setLimits'+q);
  if(d.status==='ok')toast('✓ Limits saved!','ok');
  else if(!isNetErr(d))toast(d.msg||'Error saving limits','err');
  btnIdle('saveLimitsBtn');
}
async function setEn(i){
  var v=document.getElementById('en'+i).checked?1:0;
  var d=await apiCall('/api/setEnabled?idx='+i+'&val='+v);
  if(d.status==='ok')toast('Meter '+(i+1)+' '+(v?'enabled':'disabled'),'ok');
  else if(!isNetErr(d))toast(d.msg||'Error toggling meter','err');
}
async function manSwitch(){
  var m=document.getElementById('manM').value;
  var d=await apiCall('/api/switchMeter?m='+m);
  if(d.status==='ok')toast('Switched to Meter '+(parseInt(m)+1),'ok');
  else if(!isNetErr(d))toast(d.msg||'Switch failed','err');
}
function resetEnergy(){
  confirmDo('Reset energy counters?',
    'All energy statistics (today, month, last month and the PZEM counter) will be zeroed. This cannot be undone.',
    'Reset Energy',async function(){
      var d=await apiCall('/api/resetEnergy');
      if(d.status==='ok')toast('Energy counter reset','warn');
      else if(!isNetErr(d))toast(d.msg||'Error resetting energy','err');
    });
}
// Emergency OFF — Admin-only (the /api/emergency route is authed, and apiCall
// prompts the login modal first). Strong confirmation; on success the live
// state is re-polled immediately so the badges/relays reflect the cut-off.
var _busyEmg=false;
function doEmergency(){
  if(_busyEmg)return;                    // no duplicate emergency requests
  confirmDo('Emergency OFF?',
    'WARNING: This will immediately switch EVERY controlled load OFF (all relays de-energised) and hold them safe. It does NOT restart the device. You can resume normal operation via Bypass/Reset after clearing the cause.',
    'Emergency OFF',async function(){
      _busyEmg=true;
      var d=await apiCall('/api/emergency');
      if(d.status==='ok'){
        toast('Emergency OFF activated!','err');
        poll();                          // reflect the new state right away
      }else if(!isNetErr(d))toast(d.msg||'Auth or state error','err');
      _busyEmg=false;
    });
}
// BUG FIX #2: bypass mode toggle — button reflects live state from poll()
var bypassOn=false;
function updBypassBtn(on){
  bypassOn=on;
  var b=document.getElementById('bypBtn');
  b.innerHTML='&#9193; Bypass: '+(on?'ON':'OFF');
}
// Bypass toggle — confirmation in BOTH directions (enabling suspends the
// scheduler's protection switching; disabling resumes it), then the button
// state is updated locally AND the live status is re-polled so the badge and
// any in-flight switch logic agree. Backend behaviour is unchanged.
var _busyByp=false;
function toggleBypass(){
  if(_busyByp)return;
  var turnOn=!bypassOn;
  confirmDo(turnOn?'Enable Bypass?':'Disable Bypass?',
    turnOn
      ?'Bypass suspends the automatic meter switching: the active load stays connected regardless of its energy limit. Voltage/current protection and Emergency OFF stay fully active.'
      :'Bypass will be disabled and automatic meter switching resumes. Voltage/current protection stays fully active.',
    turnOn?'Enable Bypass':'Disable Bypass',async function(){
      _busyByp=true;
      var d=await apiCall('/api/setBypass?val='+(turnOn?1:0));
      if(d.status==='ok'){
        updBypassBtn(turnOn);            // reflect the new state immediately
        toast('Bypass mode '+(turnOn?'enabled':'disabled'),'warn');
        poll();                          // reconcile with the real backend state
      }else if(!isNetErr(d))toast(d.msg||'Error setting bypass','err');
      _busyByp=false;
    });
}
// Optimistic Add Meter: show the new card immediately, keep the button busy,
// then reconcile with the real /api/status data. On failure the temp card is
// rolled back and the real error is shown — nothing is added server-side yet.
var _tmpMeter=null;
function tempMeterCard(){
  var d=document.createElement('div');
  d.className='mi fade-in';
  d.innerHTML='<div class="mi-top"><div class="mi-name"><span class="adot"></span>Meter '+(N+1)+'</div>'
    +'<span class="mt act" id="tmpMt">ADDING</span></div>'
    +'<div class="bwrap"><div class="bfill" style="width:0%"></div></div>'
    +'<div class="mstats"><span id="tmpUs">0.000 kWh</span><span id="tmpLm">/ -- kWh</span></div>';
  return d;
}
async function addMeter(){
  if(!btnBusy('addMeterBtn','Adding...'))return;      // no duplicate requests
  var tmp=tempMeterCard();
  _tmpMeter=tmp;
  document.getElementById('meterList').appendChild(tmp);
  var d=await apiCall('/api/addMeter');
  if(d.status==='ok'){
    toast('Meter '+(d.idx+1)+' added','ok');
    buildUI(d.meterCount,true);       // replaces the temp card with real data
    poll();
  }else{
    if(_tmpMeter&&_tmpMeter.parentNode)_tmpMeter.parentNode.removeChild(_tmpMeter);
    _tmpMeter=null;
    if(!isNetErr(d))toast(d.msg||'Cannot add meter','err');
  }
  btnIdle('addMeterBtn');
}
var _busyRm=false;
function removeMeter(i){
  if(_busyRm)return;                   // one removal at a time
  confirmDo('Remove Meter '+(i+1)+'?',
    'The meter slot, its limit and its usage data will be removed and remaining meters renumbered. This cannot be undone.',
    'Remove Meter',async function(){
      _busyRm=true;
      var row=document.getElementById('mi'+i);
      if(row){row.classList.add('fade-out');row.style.pointerEvents='none';}
      var d=await apiCall('/api/removeMeter?idx='+i);
      if(d.status==='ok'){
        toast('Meter removed','ok');
        buildUI(d.meterCount);
        poll();
      }else{
        if(row){row.classList.remove('fade-out');row.style.pointerEvents='';}
        if(!isNetErr(d))toast(d.msg||'Cannot remove meter','err');
      }
      _busyRm=false;
    });
}
// ===== FEATURE 12: relay test mode =====
var tmOn=false;
// Keeps every "Test Mode" toggle (System tab + Quick Actions) and its relay
// list in sync from the same status poll. Buttons share the .tm-toggle class,
// relay containers the .tm-relays class.
function updTestMode(d){
  tmOn=!!d.testMode;
  var label='&#128296; Test Mode: '+(tmOn?('ON ('+(d.testLeft||0)+'s left)'):'OFF');
  var toggles=document.querySelectorAll('.tm-toggle');
  for(var t=0;t<toggles.length;t++){
    var b=toggles[t];
    b.innerHTML=_busyBtn[b.id]?(tmOn?'Turning off...':'Turning on...'):label;
    b.className='btn '+(tmOn?'btn-red':'btn-amber')+' tm-toggle';
  }
  var boxes=document.querySelectorAll('.tm-relays');
  for(var c=0;c<boxes.length;c++){
    var r=boxes[c];
    if(tmOn&&!r.dataset.built){
      var h='';
      for(var i=0;i<N;i++){h+='<button class="btn btn-blue" style="margin-top:4px" onclick="testRelay('+i+',this)">Toggle Relay '+(i+1)+'</button>';}
      r.innerHTML=h;r.dataset.built='1';
    }
    r.style.display=tmOn?'':'none';
    if(!tmOn)delete r.dataset.built;
  }
}
function toggleTestMode(btnId){
  btnId=btnId||'tmBtn';
  if(!tmOn){
    confirmDo('Enable RELAY TEST MODE?',
      'The scheduler is disabled and relays switch ONLY by your button presses. Loads WILL be energised/de-energised. Test mode auto-exits after 5 minutes.',
      'Enable Test Mode',function(){tmRequest(true,btnId);});
  }else{tmRequest(false,btnId);}
}
async function tmRequest(on,btnId){
  if(!btnBusy(btnId,on?'Turning on...':'Turning off...'))return;
  var d=await apiCall('/api/testMode?on='+(on?1:0));
  if(d.status==='ok')toast('Test mode '+(on?'enabled':'disabled'),'warn');
  else if(!isNetErr(d))toast(d.msg||'Error toggling test mode','err');
  btnIdle(btnId);
}
// Toggle one relay in test mode. Busy-guards the button (no duplicate
// commands), and only reports the ON/OFF state returned by the backend — the
// relay is never shown as a state the firmware has not confirmed.
async function testRelay(i,el){
  var btn=el;
  if(!btn||btn.disabled)return;
  btn.disabled=true;
  var d=await apiCall('/api/testRelay?idx='+i);
  if(d.status==='ok'){
    toast('Relay '+(i+1)+' '+(d.on?'ON':'OFF'),'ok');
    btn.innerHTML='Relay '+(i+1)+': '+(d.on?'ON':'OFF');
  }else if(!isNetErr(d))toast(d.msg||'Error toggling relay','err');
  setTimeout(function(){btn.innerHTML='Toggle Relay '+(i+1);btn.disabled=false;},1500);
}
// ===== FEATURE 14: browser time sync =====
// btnId/resId are optional — the System tab calls syncBrowserTime() with no
// args; the Quick Actions button passes its button + result element so the
// user sees progress and the newly synced device time.
async function syncBrowserTime(btnId,resId){
  if(btnId&&!btnBusy(btnId,'Syncing...'))return;
  var n=new Date();
  var q='/api/setTime?y='+n.getFullYear()+'&mo='+(n.getMonth()+1)+'&d='+n.getDate()
       +'&h='+n.getHours()+'&mi='+n.getMinutes()+'&s='+n.getSeconds();
  var d=await apiCall(q);
  if(d.status==='ok'){
    var t=timeNow();
    toast('Time synced from browser','ok');
    if(resId){var r=document.getElementById(resId);if(r)r.textContent='RTC synced: '+t;}
  }else if(!isNetErr(d))toast(d.msg||'Time sync failed','err');
  if(btnId)btnIdle(btnId);
  poll();loadSys();
}
// ===== FEATURE 8: backup / restore =====
async function doRestore(inp){
  var f=inp.files[0];if(!f)return;
  var txt=await f.text();inp.value='';
  try{JSON.parse(txt);}catch(e){toast('Not a valid JSON file','err');return;}
  confirmDo('Import this configuration?',
    'Current settings (meters, limits, protection, schedules, WiFi) will be OVERWRITTEN by the imported file. Export a backup first if you need the current config.',
    'Import Config',async function(){
      var d=await apiCall('/api/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:txt});
      if(d.status==='ok')toast('Config restored!','ok');
      else if(!isNetErr(d))toast(d.msg||'Restore failed','err');
      if(d.status==='ok')setTimeout(poll,500);
    });
}
// Restart — Admin-only (authed route). The button shows "Restarting..." and
// success is only reported after the device actually replies ok (which the
// backend sends right before the deferred reboot), so a dropped connection
// before the acknowledgement is never shown as a successful restart.
function doRestart(btnId){
  confirmDo('Restart the ESP32?',
    'The device will reboot in about 1&ndash;2 seconds. All settings, meters, limits and NVS data are preserved. The dashboard will reconnect automatically.',
    'Restart Now',async function(){
      if(btnId&&!btnBusy(btnId,'&#128260; Restarting...'))return;
      var d=await apiCall('/api/restart');
      if(d.status==='ok'){
        toast('Restart accepted — device is rebooting...','warn');
        if(btnId){var b=document.getElementById(btnId);if(b)b.innerHTML='&#128260; Restarting...';}
        setTimeout(poll,4000);
      }else{
        if(btnId)btnIdle(btnId);
        if(!isNetErr(d))toast(d.msg||'Restart failed','err');
      }
    });
}
// ===== FEATURE 9: factory reset =====
function doFactoryReset(){
  confirmDo('Factory reset?',
    'ALL settings, statistics and WiFi credentials will be erased from NVS and the device will reboot with factory defaults. This CANNOT be undone.',
    'Factory Reset',function(){
      confirmDo('Are you absolutely sure?',
        'Every meter, limit, protection threshold, schedule and WiFi credential will be lost and the device reboots with defaults.',
        'Yes, erase everything',async function(){
          var d=await apiCall('/api/factoryReset');
          if(d.status==='ok')toast('Factory reset — device rebooting...','warn');
          else if(!isNetErr(d))toast(d.msg||'Factory reset error','err');
        });
    });
}
// ===== FEATURE 7: browser OTA upload =====
var _otaBusy=false;
async function doOta(inp){
  var f=inp.files[0];if(!f)return;
  inp.value='';
  if(_otaBusy){toast('Upload already in progress','err');return;}
  confirmDo('Flash "'+f.name+'" ('+Math.round(f.size/1024)+' KB)?',
    'The device will be overwritten with this firmware and will REBOOT automatically on success. Do not disconnect power or browse away during the upload.',
    'Start Upload',function(){startOta(f);});
}
async function startOta(f){
  _otaBusy=true;
  var obtn=document.getElementById('otaBtn');
  if(obtn){obtn.disabled=true;obtn.innerHTML='Uploading...';}
  var bar=document.getElementById('otaBar');
  var msg=document.getElementById('otaMsg');
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/update');
  xhr.timeout=180000;                    // slow flashing still has a hard cap
  xhr.ontimeout=function(){_otaBusy=false;if(obtn){obtn.disabled=false;obtn.innerHTML='&#128190; Upload .bin';}bar.style.width='0%';msg.textContent='Upload timed out';toast('OTA timed out','err');};
  var ah=await authHdr();if(ah.Authorization)xhr.setRequestHeader('Authorization',ah.Authorization);
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);bar.style.width=p+'%';msg.textContent='Uploading... '+p+'%';}};
  xhr.onload=function(){
    if(xhr.status===401){checkAuth({status:401});_otaBusy=false;if(obtn){obtn.disabled=false;obtn.innerHTML='&#128190; Upload .bin';}bar.style.width='0%';msg.textContent='Auth required — retry';toast('Auth required — try again','err');return;}
    try{var d=JSON.parse(xhr.responseText);}catch(e){var d={};}
    if(xhr.status===200&&d.status==='ok'){msg.textContent='Success! Rebooting...';toast('Firmware updated — rebooting','ok');}
    else{_otaBusy=false;if(obtn){obtn.disabled=false;obtn.innerHTML='&#128190; Upload .bin';}bar.style.width='0%';msg.textContent='Failed: '+(d.msg||'error');toast('OTA failed: '+(d.msg||'error'),'err');}
  };
  xhr.onerror=function(){_otaBusy=false;if(obtn){obtn.disabled=false;obtn.innerHTML='&#128190; Upload .bin';}bar.style.width='0%';msg.textContent='Upload error';toast('OTA upload error','err');};
  var fd=new FormData();fd.append('firmware',f,f.name);
  xhr.send(fd);
}
// ===== FEATURE 10: event log viewer (categorized, filterable, searchable) =====
// The firmware logs flat messages with no category field, so the dashboard
// derives a category from the existing message text. Only the message the
// backend already emits is used — nothing new is invented on the firmware
// side, and every existing event keeps appearing verbatim.
var _evCache=null;      // last-good parsed snapshot; kept across load failures
var _evFilter='all';    // all | info | warn | fault | sec | sys
var _evErr=false;
var EV_LABEL={all:'All',info:'Info',warn:'Warning',fault:'Fault',sec:'Security',sys:'System'};
var EV_ICON={all:'&#128203;',info:'&#8505;',warn:'&#9888;',fault:'&#9889;',sec:'&#128274;',sys:'&#9881;'};
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];});}
// Map an existing event message to a category. Fault (trips/recovery/emergency)
// and security (auth) checks run first so e.g. "OTA rejected: auth failed"
// is a Security event, not a generic warning.
function evCat(m){
  m=m||'';
  if(/trip:|recovery|fault|emergency/i.test(m))return 'fault';
  if(/auth failed|network forgotten/i.test(m))return 'sec';
  if(/pzem reset|wifi disconnected|ota aborted|ota rejected|ota failed/i.test(m))return 'warn';
  if(/boot|event log cleared|config restored|time set from browser|monthly reset|relay test mode|test mode timeout|ntp time synced/i.test(m))return 'sys';
  return 'info';
}
// Pull the related meter/relay and the relevant value out of the message
// when the backend already put them there (e.g. "Meter 3 added",
// "Trip: Over Voltage: 250.0V (limit 245V)", "PZEM reset: 1.20->0.00 kWh").
function evDetail(m){
  m=m||'';
  var load='',val='';
  var lm=m.match(/Meter\s+(\d+)/i);
  var lr=m.match(/Relay\s+(\d+)/i);
  var tr=m.match(/relay\s+(\d+)/i);
  if(lm)load='Meter '+lm[1];
  else if(lr)load='Relay '+lr[1];
  else if(tr)load='Relay '+tr[1];
  var pz=m.match(/PZEM reset:\s*([\d.]+)->([\d.]+)\s*kWh/i);
  var kw=m.match(/\d+(?:\.\d+)?\s*kWh/i);
  var vv=m.match(/\d+(?:\.\d+)?\s*V/i);
  var ca=m.match(/\d+(?:\.\d+)?\s*A/i);
  var rs=m.match(/Recovery started \((\d+)s\)/i);
  var ip=m.match(/\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b/);
  if(pz)val=pz[1]+' -> '+pz[2]+' kWh';
  else if(kw)val=kw[0];
  else if(vv)val=vv[0];
  else if(ca)val=ca[0];
  else if(rs)val=rs[1]+'s';
  else if(ip)val=ip[1];
  return {load:load,val:val};
}
function evFilter(cat){
  _evFilter=cat;
  var btns=document.querySelectorAll('#evFilters .evf');
  for(var i=0;i<btns.length;i++)btns[i].className='evf'+(btns[i].getAttribute('data-cat')===cat?' active':'');
  evRender();
}
function evRetry(){loadEvents(true);}
// Render the cached snapshot through the active filter + search. Newest events
// first (the API sends oldest-first). Falls back to a clear empty/error state.
function evRender(){
  var el=document.getElementById('evList');
  if(!el)return;
  var q=(document.getElementById('evSearch')?document.getElementById('evSearch').value:'').trim().toLowerCase();
  var rows=_evCache?_evCache.slice().reverse():[];
  if(_evFilter!=='all')rows=rows.filter(function(e){return e.cat===_evFilter;});
  if(q)rows=rows.filter(function(e){
    var hay=(e.m+' '+(e.detail.load||'')+' '+(e.detail.val||'')+' '+e.t).toLowerCase();
    return hay.indexOf(q)>=0;
  });
  if(_evErr&&!(_evCache&&_evCache.length)){
    el.innerHTML='<div class="ev-empty">Could not load the event log.<br><button class="btn btn-blue" style="width:auto;margin:12px auto 0;padding:6px 16px" onclick="evRetry()">&#8635; Retry</button></div>';
    return;
  }
  if(!_evCache||!_evCache.length){
    el.innerHTML='<div class="ev-empty">No events recorded yet.</div>';
    return;
  }
  if(!rows.length){
    el.innerHTML='<div class="ev-empty">No events match the current filter'+(q?' and search':'')+'.</div>';
    return;
  }
  var h='';
  for(var i=0;i<rows.length;i++){
    var e=rows[i];
    var meta=(e.detail.load||e.detail.val)?('<div class="ev-meta">'+(e.detail.load?esc(e.detail.load):'')+(e.detail.load&&e.detail.val?' &middot; ':'')+(e.detail.val?esc(e.detail.val):'')+'</div>'):'';
    var parts=String(e.t).split(' ');
    var date=parts.length>1?esc(parts[0]):'';
    var time=parts.length>1?esc(parts[1]):esc(e.t);
    h+='<div class="ev-row">'
      +'<span class="ev-badge ev-'+e.cat+'">'+EV_ICON[e.cat]+' '+EV_LABEL[e.cat]+'</span>'
      +'<div class="ev-main"><div class="ev-desc">'+esc(e.m)+'</div>'+meta+'</div>'
      +'<div class="ev-ts">'+(date?'<span class="ev-date">'+date+'</span>':'')+'<span class="ev-time">'+time+'</span></div>'
      +'</div>';
  }
  el.innerHTML=h;
}
async function loadEvents(force){
  // Only refresh while the log is actually visible: the System tab is the only
  // place it appears, and a background tab must not keep downloading it.
  if(document.hidden)return;
  if(!force&&_curTab!=='system')return;
  try{
    var r=await fetch('/api/events');
    if(!r.ok)throw new Error('HTTP '+r.status);
    var d=await r.json();
    var arr=d.events||[];
    _evCache=[];
    for(var i=0;i<arr.length;i++){
      _evCache.push({t:arr[i].t,m:arr[i].m,cat:evCat(arr[i].m),detail:evDetail(arr[i].m)});
    }
    _evErr=false;
    var eb=document.getElementById('evErr');
    if(eb)eb.style.display='none';
    evRender();
  }catch(e){
    // Keep whatever was already rendered; surface a clear error + retry.
    _evErr=true;
    var eb=document.getElementById('evErr');
    if(eb)eb.style.display='flex';
    evRender();
  }
}
function clearEvents(){
  confirmDo('Clear the event log?',
    'Every event entry (faults, switching, OTA, PZEM resets) will be erased from memory. This cannot be undone.',
    'Clear Log',async function(){
      var d=await apiCall('/api/clearEvents');
      if(d.status==='ok'){toast('Event log cleared','warn');_evCache=null;loadEvents(true);}
      else if(!isNetErr(d))toast(d.msg||'Error clearing events','err');
    });
}
// ===== FEATURE 14/14: reports & CSV export =====
// Exports only what the dashboard already reads (/api/events, /api/status).
// Never /api/backup: that is the config dump and may carry credentials, so it
// is deliberately not used. Rows are quoted/escaped for CSV; the file is
// generated in-browser with a Blob, so a large export never blocks the UI.
var _exportBusy=false;   // one export at a time (shares the btnBusy lock)
function expStamp(){var d=new Date();function p(n){return (n<10?'0':'')+n;}return d.getFullYear()+'-'+p(d.getMonth()+1)+'-'+p(d.getDate());}
function csvCell(v){
  v=(v==null)?'':String(v);
  if(/[",\r\n]/.test(v))return '"'+v.replace(/"/g,'""')+'"';
  return v;
}
function downloadCsv(name,content){
  var blob=new Blob([content],{type:'text/csv;charset=utf-8'});
  var url=URL.createObjectURL(blob);
  var a=document.createElement('a');
  a.href=url;a.download=name;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  setTimeout(function(){URL.revokeObjectURL(url);},1000);
}
function expRangeUI(){
  var c=document.getElementById('expCustomRange');
  if(c)c.style.display=(document.getElementById('expRange').value==='custom')?'':'none';
}
async function exportEventsCSV(btnId){
  if(_exportBusy)return;
  if(btnId&&!btnBusy(btnId,'Preparing...'))return;
  _exportBusy=true;
  try{
    var r=await fetch('/api/events');
    if(!r.ok)throw new Error('HTTP '+r.status);
    var d=await r.json();
    var evs=d.events||[];
    var lines=['Date,Time,Type,Description,Meter/Load,Value'];
    for(var i=0;i<evs.length;i++){
      var t=evs[i].t||'',m=evs[i].m||'',dt='',tm=t;
      var sp=t.indexOf(' ');
      if(sp>0){dt=t.slice(0,sp);tm=t.slice(sp+1);}
      var det=evDetail(m);
      lines.push([csvCell(dt),csvCell(tm),csvCell(EV_LABEL[evCat(m)]||'Info'),csvCell(m),csvCell(det.load),csvCell(det.val)].join(','));
    }
    downloadCsv('SmartMeterATS_EventLog_'+expStamp()+'.csv',lines.join('\r\n')+'\r\n');
    toast('Event log exported ('+evs.length+' events)','ok');
  }catch(e){
    toast('Export failed: '+e.message,'err');
  }finally{
    _exportBusy=false;
    if(btnId)btnIdle(btnId);
  }
}
async function exportEnergyCSV(btnId){
  if(_exportBusy)return;
  if(btnId&&!btnBusy(btnId,'Preparing...'))return;
  _exportBusy=true;
  try{
    var r=await fetch('/api/status');
    if(!r.ok)throw new Error('HTTP '+r.status);
    var d=await r.json();
    var range=document.getElementById('expRange').value;
    var from=(document.getElementById('expFrom').value||'').replace(/-/g,'');
    var to=(document.getElementById('expTo').value||'').replace(/-/g,'');
    var lines=[];
    lines.push('SmartMeterATS Energy Report');
    lines.push('Generated,'+csvCell(expStamp()+' '+new Date().toTimeString().slice(0,8)));
    lines.push('Range,'+csvCell({today:'Today','7d':'Last 7 Days','30d':'Last 30 Days',custom:'Custom'}[range]||range));
    lines.push('');
    lines.push('Live Snapshot');
    lines.push('Voltage (V),Current (A),Power (W),Energy (kWh)');
    lines.push([csvCell(d.voltage),csvCell(d.current),csvCell(d.power),csvCell(d.energy)].join(','));
    lines.push('');
    lines.push('Period Totals (kWh)');
    lines.push('Today,This Month,Last Month');
    lines.push([csvCell(d.todayUsed),csvCell(d.thisMonth),csvCell(d.lastMonth)].join(','));
    lines.push('');
    lines.push('Meter Summary');
    lines.push('Meter,Used (kWh),Limit (kWh),Enabled,Active');
    for(var i=0;i<(d.meterCount||0);i++){
      lines.push([csvCell(i+1),csvCell(d.used&&d.used[i]),csvCell(d.limits&&d.limits[i]),csvCell(d.enabled&&d.enabled[i]?'yes':'no'),csvCell(d.activeMeter===i?'yes':'no')].join(','));
    }
    lines.push('');
    lines.push('Daily Usage (kWh)');
    lines.push('Date,Energy (kWh)');
    var daily=d.daily||[];
    // daily[] is oldest-first, current day LAST. The device RTC date (when
    // present) labels the newest slot; earlier slots are counted back from it.
    // That keeps the exported history on the device's own calendar. Without
    // an RTC the browser date stands in for "today" — never fabricated data.
    var base=new Date();
    if(d.rtcYear&&d.rtcMonth&&d.rtcDay)base=new Date(d.rtcYear,d.rtcMonth-1,d.rtcDay);
    for(var j=0;j<daily.length;j++){
      var dd=new Date(base);
      dd.setDate(base.getDate()-(daily.length-1-j));
      var ds=dd.getFullYear()+''+((dd.getMonth()+1)<10?'0':'')+(dd.getMonth()+1)+''+(dd.getDate()<10?'0':'')+dd.getDate();
      var ys=ds.slice(0,4)+'-'+ds.slice(4,6)+'-'+ds.slice(6,8);
      var keep = range==='30d' || (range==='today'&&j===daily.length-1) || (range==='7d'&&j>=daily.length-7) ||
                 (range==='custom'&&(!from||ds>=from)&&(!to||ds<=to));
      if(keep)lines.push(csvCell(ys)+','+csvCell(daily[j]));
    }
    downloadCsv('SmartMeterATS_Energy_'+expStamp()+'.csv',lines.join('\r\n')+'\r\n');
    toast('Energy report exported','ok');
  }catch(e){
    toast('Export failed: '+e.message,'err');
  }finally{
    _exportBusy=false;
    if(btnId)btnIdle(btnId);
  }
}
// ===== FEATURE 11/13: system info + RTC health (auto-refresh) =====
function sysItem(l,v){return '<div class="stat-item"><div class="stat-val" style="font-size:0.95rem">'+v+'</div><div class="stat-lbl">'+l+'</div></div>';}
async function loadSys(){
  if(document.hidden)return;
  try{
    var r=await fetch('/api/sysinfo');
    if(!r.ok)throw new Error('HTTP '+r.status);
    var d=await r.json();
    var up=d.uptime,dd=Math.floor(up/86400),hh=Math.floor(up%86400/3600),mm=Math.floor(up%3600/60);
    document.getElementById('sysGrid').innerHTML=
      sysItem('Firmware','v'+d.fw)+sysItem('Build',d.build)+sysItem('Chip',d.chip)
      +sysItem('Flash',(d.flash/1048576)+' MB')+sysItem('Free Heap',Math.round(d.heap/1024)+' KB')
      +sysItem('CPU',d.cpu+' MHz')+sysItem('Uptime',dd+'d '+hh+'h '+mm+'m')
      +sysItem('Restart Reason',d.rstReason)+sysItem('WiFi RSSI',d.staRssi?d.staRssi+' dBm':'--')
      +sysItem('IP',d.ip||'--')+sysItem('Time Source',d.timeSrc)+sysItem('Active Meters',d.meterCount);
    document.getElementById('accessInfo').textContent=d.hostname?'Access: http://'+d.hostname+'/':'Access: http://'+d.ip+'/';
    document.getElementById('rtcInfo').textContent=
      (d.rtcOK?('RTC OK'+(d.rtcLostPower?' (battery LOW — time was lost!)':' (battery OK)')+' • '+d.rtcTime):'RTC MISSING — check wiring')
      +(d.lastSync?' • Last sync: '+d.lastSync:'');
  }catch(e){
    var sg=document.getElementById('sysGrid');
    if(sg)sg.innerHTML='<div style="grid-column:1/-1;color:#fca5a5;text-align:center;padding:16px">System info unavailable.</div>';
  }
}
showLogin();      // unlock up front; dismissable — read-only pages work locked
showTab('overview');
tickClock();setInterval(tickClock,1000);
poll();
setInterval(poll,3000);
loadSys();
setInterval(loadSys,10000);
// Event log polling is tab-aware: loadEvents() no-ops unless the System tab is
// open (and the page is visible), so the log is never downloaded while the user
// is elsewhere or the tab is backgrounded.
setInterval(loadEvents,20000);
// Refresh immediately when the tab regains focus — the background-tab guard in
// poll()/loadEvents()/loadSys() skips DOM work while hidden, so this catches up.
document.addEventListener('visibilitychange',function(){
  if(!document.hidden){poll();loadEvents();loadSys();}
});
// Always re-sync the clock and status when the page is refocused (window blur
// on the ESP32 dashboard tends to be a modal, not a tab switch).
window.addEventListener('focus',function(){if(!document.hidden){poll();}});
</script>
</body>
</html>
)rawhtml";

}  // namespace ui

#endif  // UI_DASHBOARD_H