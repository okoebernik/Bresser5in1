#pragma once
#include <Arduino.h>

// Web-UI im Look der Clinometer-/Daemmerungsschalter-Geraete:
// Statusleiste oben, Sidebar, Karten, EN/DE-Umschaltung (Standard EN, im Browser gespeichert).
// Handgeschriebenes Vanilla-HTML/CSS/JS, kein Build-Schritt.

const char WEB_CSS[] PROGMEM = R"CSS(
:root{color-scheme:dark;--bg:#0b121a;--bg-elevated:#131d29;--bg-inset:#1a2733;--border:#253442;--text:#e8eef5;--text-muted:#8a99a8;--accent:#4da3ff;--accent-contrast:#041322;--ok:#3ecf8e;--warn:#e0a030;--danger:#e0473c;--radius-lg:14px;--radius-md:10px;--radius-sm:7px;--shadow:0 1px 2px rgba(0,0,0,.4),0 8px 24px -12px rgba(0,0,0,.5);--sidebar-w:220px;--statusbar-h:38px}
*{box-sizing:border-box}html,body{height:100%}
body{margin:0;padding-top:var(--statusbar-h);font-family:-apple-system,"Segoe UI",system-ui,Roboto,sans-serif;background:var(--bg);color:var(--text);-webkit-font-smoothing:antialiased}
.statusbar{position:fixed;top:0;left:0;right:0;height:var(--statusbar-h);z-index:50;display:flex;align-items:center;gap:1.1rem;padding:0 1rem;background:var(--bg-elevated);border-bottom:1px solid var(--border);font-size:.78rem;color:var(--text-muted);overflow-x:auto;white-space:nowrap}
.statusbar__item{display:inline-flex;align-items:center;gap:.4rem;flex-shrink:0}
.statusbar__dot{width:7px;height:7px;border-radius:50%;background:var(--text-muted);flex-shrink:0;transition:background .2s}
.statusbar__dot.ok{background:var(--ok);box-shadow:0 0 6px color-mix(in srgb,var(--ok) 60%,transparent)}
.statusbar__dot.warn{background:var(--warn)}
.statusbar__value{color:var(--text);font-weight:600}
.statusbar__spacer{flex:1}
.lang-select{background:var(--bg-inset);border:1px solid var(--border);color:var(--text);border-radius:var(--radius-sm);padding:.15rem .4rem;font-size:.76rem;font-family:inherit;cursor:pointer}
.app{display:flex;min-height:100vh}
.sidebar{width:var(--sidebar-w);flex-shrink:0;background:var(--bg-elevated);border-right:1px solid var(--border);display:flex;flex-direction:column;position:fixed;top:var(--statusbar-h);bottom:0;left:0;transition:transform .2s ease;z-index:40}
.sidebar__brand{padding:1.15rem 1.2rem;font-weight:700;font-size:1.02rem;border-bottom:1px solid var(--border);text-align:center}
.nav{flex:1;padding:.75rem .6rem;overflow-y:auto}
.nav__item{display:flex;align-items:center;gap:.7rem;width:100%;padding:.6rem .75rem;margin-bottom:.15rem;border:none;background:none;color:var(--text-muted);border-radius:var(--radius-sm);font-size:.92rem;font-family:inherit;cursor:pointer;text-align:left}
.nav__item:hover{background:var(--bg-inset);color:var(--text)}
.nav__item.active{background:color-mix(in srgb,var(--accent) 15%,transparent);color:var(--accent);font-weight:600}
.nav__item svg{width:18px;height:18px;flex-shrink:0}
.sidebar__footer{padding:.9rem 1.2rem;border-top:1px solid var(--border);font-size:.76rem;color:var(--text-muted)}
.main{flex:1;margin-left:var(--sidebar-w);min-width:0;padding:1.6rem 2rem 3rem}
.topbar{display:flex;align-items:center;justify-content:space-between;gap:1rem;margin-bottom:1.5rem}
.topbar h1{font-size:1.35rem;margin:0}
.menu-btn{display:none;border:1px solid var(--border);background:var(--bg-elevated);border-radius:var(--radius-sm);padding:.45rem .6rem;color:var(--text);cursor:pointer}
.section{display:none}.section.active{display:block}
.section-lead{color:var(--text-muted);font-size:.9rem;margin:-.75rem 0 1.25rem}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:1rem}
.card{background:var(--bg-elevated);border:1px solid var(--border);border-radius:var(--radius-lg);padding:1.1rem 1.25rem;box-shadow:var(--shadow)}
.card__subtitle{color:var(--text-muted);font-size:.76rem;text-transform:uppercase;letter-spacing:.04em;margin-bottom:.3rem}
.card__value{font-size:1.7rem;font-weight:700;line-height:1.15;font-variant-numeric:tabular-nums}
.card__value small{font-size:.95rem;font-weight:500;color:var(--text-muted)}
.card__foot{margin-top:.5rem;font-size:.78rem;color:var(--text-muted)}
.card--wide{grid-column:1/-1;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:1rem}
.panel{background:var(--bg-elevated);border:1px solid var(--border);border-radius:var(--radius-lg);padding:1.25rem 1.4rem;box-shadow:var(--shadow);margin-bottom:1.25rem}
.panel__header{display:flex;align-items:center;justify-content:space-between;gap:1rem;margin-bottom:1rem}
.panel__header h2{margin:0;font-size:1.05rem}
.panel__desc{color:var(--text-muted);font-size:.85rem;margin:-.5rem 0 1rem}
.badge{display:inline-flex;align-items:center;gap:.35rem;padding:.2rem .6rem;border-radius:999px;font-size:.76rem;font-weight:600;background:var(--bg-inset);color:var(--text-muted);border:1px solid var(--border)}
.badge.ok{color:var(--ok);border-color:color-mix(in srgb,var(--ok) 40%,var(--border))}
.badge.warn{color:var(--warn);border-color:color-mix(in srgb,var(--warn) 40%,var(--border))}
.badge.bad{color:var(--danger);border-color:color-mix(in srgb,var(--danger) 40%,var(--border))}
.badge .dot{width:6px;height:6px;border-radius:50%;background:currentColor}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:.4rem;border:1px solid var(--border);background:var(--bg-inset);color:var(--text);border-radius:var(--radius-sm);padding:.55rem 1rem;font-size:.88rem;font-family:inherit;font-weight:600;cursor:pointer;transition:filter .1s;white-space:nowrap}
.btn:hover{filter:brightness(1.12)}.btn:disabled{opacity:.5;cursor:not-allowed}
.btn.btn--primary{background:var(--accent);border-color:var(--accent);color:var(--accent-contrast)}
.btn.btn--danger{background:var(--danger);border-color:var(--danger);color:#fff}
.field{margin-bottom:.95rem}
.field label{display:block;font-size:.82rem;color:var(--text-muted);margin-bottom:.35rem}
.field input[type=text],.field input[type=password],.field input[type=number],.field select{width:100%;background:var(--bg-inset);border:1px solid var(--border);color:var(--text);border-radius:var(--radius-sm);padding:.55rem .7rem;font-size:.9rem;font-family:inherit}
.field input:focus,.field select:focus{outline:none;border-color:var(--accent)}
.field--row{display:grid;grid-template-columns:1fr 1fr;gap:.9rem}
.field-hint{color:var(--text-muted);font-size:.76rem;margin-top:.3rem}
.checkbox-row{display:flex;align-items:center;gap:.55rem;margin:.9rem 0}
.checkbox-row input{width:auto;accent-color:var(--accent)}
.checkbox-row label{margin:0;color:var(--text);font-size:.9rem}
.form-actions{display:flex;gap:.6rem;justify-content:flex-end;margin-top:1.1rem;padding-top:1rem;border-top:1px solid var(--border)}
.scan-results{margin-bottom:.9rem;border:1px solid var(--border);border-radius:var(--radius-sm);max-height:220px;overflow-y:auto}
.scan-results__item{display:flex;justify-content:space-between;align-items:center;gap:.6rem;padding:.5rem .7rem;cursor:pointer;font-size:.85rem;border-bottom:1px solid var(--border);background:var(--bg-inset)}
.scan-results__item:last-child{border-bottom:none}.scan-results__item:hover{filter:brightness(1.15)}
.scan-results__meta{color:var(--text-muted);font-size:.78rem;white-space:nowrap}
.console{margin:0 0 .8rem;height:58vh;min-height:260px;overflow:auto;background:#070c12;border:1px solid var(--border);border-radius:var(--radius-sm);padding:.7rem .8rem;font:12.5px/1.45 ui-monospace,Consolas,monospace;color:var(--text);white-space:pre-wrap;word-break:break-word}
.console .l-sensor{color:var(--ok)}.console .l-other{color:var(--accent)}.console .l-warn{color:var(--warn)}.console .l-err{color:var(--danger)}.console .l-dim{color:var(--text-muted)}
.muted{color:var(--text-muted)}.mono{font-family:ui-monospace,monospace}
.toasts{position:fixed;bottom:1.2rem;right:1.2rem;display:flex;flex-direction:column;gap:.5rem;z-index:100}
.toast{background:var(--bg-elevated);border:1px solid var(--border);border-left:3px solid var(--accent);border-radius:var(--radius-sm);padding:.7rem 1rem;font-size:.86rem;box-shadow:var(--shadow);min-width:220px;max-width:340px;animation:toast-in .15s ease}
.toast.error{border-left-color:var(--danger)}.toast.success{border-left-color:var(--ok)}
@keyframes toast-in{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:translateY(0)}}
.windrose{width:124px;height:124px;display:block}
.rose-ring{fill:var(--bg-inset);stroke:var(--border);stroke-width:1.5}.rose-axis{stroke:var(--border);stroke-width:1}
.rose-tick{stroke:var(--text-muted);stroke-width:1.4;stroke-linecap:round}
.rose-label{fill:var(--text-muted);font-size:9px;font-weight:800;text-anchor:middle;dominant-baseline:middle}
.rose-label.main{fill:var(--text);font-size:11px}
.rose-arrow{fill:var(--danger);stroke:#ffd0d0;stroke-width:.7}.rose-center{fill:var(--text);stroke:var(--bg-inset);stroke-width:1}
@media(max-width:880px){.sidebar{transform:translateX(-100%)}.sidebar.open{transform:translateX(0)}.main{margin-left:0;padding:1.1rem 1.1rem 3rem}.menu-btn{display:inline-flex}.field--row{grid-template-columns:1fr}.statusbar{gap:.7rem;padding:0 .6rem;font-size:.72rem}}
)CSS";

const char WEB_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Bresser Weather Receiver</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<div class="statusbar">
  <div class="statusbar__item"><span class="statusbar__dot" id="sb-wifi-dot"></span><span data-i18n="statusbar.wifi">WiFi</span><span class="statusbar__value" id="sb-wifi">&ndash;</span></div>
  <div class="statusbar__item"><span class="statusbar__dot" id="sb-mqtt-dot"></span><span>MQTT</span><span class="statusbar__value" id="sb-mqtt">&ndash;</span></div>
  <div class="statusbar__item"><span class="statusbar__dot" id="sb-sensor-dot"></span><span data-i18n="statusbar.sensor">Sensor</span><span class="statusbar__value mono" id="sb-sensor">&ndash;</span></div>
  <div class="statusbar__spacer"></div>
  <div class="statusbar__item">
    <select id="lang-select" class="lang-select" aria-label="Language"><option value="en">EN</option><option value="de">DE</option></select>
  </div>
</div>
<div class="app">
  <aside class="sidebar" id="sidebar">
    <div class="sidebar__brand" data-i18n="app.name">Bresser Weather Receiver</div>
    <nav class="nav" id="nav">
      <button class="nav__item" data-route="dashboard">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M7 18a4 4 0 0 1-.5-7.97A5.5 5.5 0 0 1 17 8.5 4 4 0 0 1 17 18z"/></svg>
        <span data-i18n="nav.dashboard">Weather</span>
      </button>
      <button class="nav__item" data-route="sensor">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="2"/><path d="M8 8a5.7 5.7 0 0 0 0 8M16 8a5.7 5.7 0 0 1 0 8M5 5a10 10 0 0 0 0 14M19 5a10 10 0 0 1 0 14"/></svg>
        <span data-i18n="nav.sensor">Sensor</span>
      </button>
      <button class="nav__item" data-route="network">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12.5a10 10 0 0 1 14 0M8 15.5a6 6 0 0 1 8 0M11.5 18.5a2 2 0 0 1 1 0"/><circle cx="12" cy="19" r="0.6" fill="currentColor" stroke="none"/></svg>
        <span data-i18n="nav.network">Network</span>
      </button>
      <button class="nav__item" data-route="mqtt">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="5" cy="6" r="2"/><circle cx="19" cy="6" r="2"/><circle cx="12" cy="18" r="2"/><path d="M5 8v3a2 2 0 0 0 2 2h3M19 8v3a2 2 0 0 1-2 2h-3M12 13v3"/></svg>
        <span data-i18n="nav.mqtt">MQTT</span>
      </button>
      <button class="nav__item" data-route="console">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="4" width="18" height="16" rx="2"/><path d="M7 9l3 3-3 3M13 15h4"/></svg>
        <span data-i18n="nav.console">Console</span>
      </button>
      <button class="nav__item" data-route="system">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 3v12M7 10l5 5 5-5M5 20h14"/></svg>
        <span data-i18n="nav.system">System</span>
      </button>
    </nav>
    <div class="sidebar__footer"><span data-i18n="footer.tagline">Heltec WiFi LoRa 32 V2 &middot; Bresser 5-in-1</span><br><span id="footer-ip">&ndash;</span></div>
  </aside>

  <main class="main">
    <div class="topbar">
      <div style="display:flex;align-items:center;gap:.6rem">
        <button class="menu-btn" id="menu-btn" aria-label="Menu"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M3 12h18M3 18h18"/></svg></button>
        <h1 id="page-title">Weather</h1>
      </div>
      <span class="badge" id="live-badge">&ndash;</span>
    </div>

    <section class="section" id="section-dashboard">
      <p class="section-lead" data-i18n="dashboard.lead">Live values from the Bresser 5-in-1 weather station.</p>
      <div class="grid">
        <div class="card"><div class="card__subtitle" data-i18n="d.temp">Temperature</div><div class="card__value"><span id="v-temp">&ndash;</span> <small>&deg;C</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.hum">Humidity</div><div class="card__value"><span id="v-hum">&ndash;</span> <small>%</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.dew">Dew point</div><div class="card__value"><span id="v-dew">&ndash;</span> <small>&deg;C</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.wind">Wind</div><div class="card__value"><span id="v-wind">&ndash;</span> <small>m/s</small></div><div class="card__foot" id="v-kmh">&ndash;</div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.gust">Gust</div><div class="card__value"><span id="v-gust">&ndash;</span> <small>m/s</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.rain">Rain total</div><div class="card__value"><span id="v-rain">&ndash;</span> <small>mm</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.rain1h">Rain 1 h</div><div class="card__value"><span id="v-rain1h">&ndash;</span> <small>mm</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.rain24h">Rain 24 h</div><div class="card__value"><span id="v-rain24h">&ndash;</span> <small>mm</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.rain7d">Rain 7 d</div><div class="card__value"><span id="v-rain7d">&ndash;</span> <small>mm</small></div></div>
        <div class="card"><div class="card__subtitle" data-i18n="d.battery">Battery</div><div class="card__value" id="v-batt">&ndash;</div><div class="card__foot" id="v-rssi">RSSI &ndash;</div></div>
        <div class="card card--wide">
          <div><div class="card__subtitle" data-i18n="d.dir">Wind direction</div><div class="card__value"><span id="v-dirtext">&ndash;</span> <small id="v-dirdeg">&ndash;</small></div></div>
          <svg class="windrose" viewBox="0 0 104 104" aria-label="Wind rose">
            <circle class="rose-ring" cx="52" cy="52" r="50"/>
            <path class="rose-axis" d="M52 8v88M8 52h88M20.9 20.9l62.2 62.2M83.1 20.9L20.9 83.1"/>
            <path class="rose-tick" d="M52 4v8M52 92v8M4 52h8M92 52h8M18.1 18.1l5.7 5.7M80.2 80.2l5.7 5.7M85.9 18.1l-5.7 5.7M23.8 80.2l-5.7 5.7"/>
            <text class="rose-label main" x="52" y="16">N</text><text class="rose-label main" id="rose-e" x="88" y="52">E</text><text class="rose-label main" x="52" y="88">S</text><text class="rose-label main" x="16" y="52">W</text>
            <g id="wind-arrow" class="rose-arrow"><path d="M52 12l7 38h-4v28l-3 7-3-7V50h-4z"/></g>
            <circle class="rose-center" cx="52" cy="52" r="4"/>
          </svg>
        </div>
      </div>
    </section>

    <section class="section" id="section-sensor">
      <p class="section-lead" data-i18n="sensor.lead">If several weather stations are in range, you can restrict reception to one sensor ID.</p>
      <div class="panel">
        <div class="panel__header"><h2 data-i18n="sensor.heading">Sensor selection</h2></div>
        <div class="checkbox-row"><input type="checkbox" id="f-filter"><label for="f-filter" data-i18n="sensor.filter">Use only the selected sensor ID</label></div>
        <div class="field"><label for="f-sensor" data-i18n="sensor.id">Sensor ID</label><select id="f-sensor"></select><div class="field-hint" id="seen-hint"></div></div>
        <div class="field-hint" data-i18n="sensor.hint">While the filter is active, other stations are blocked in the receiver. Disable it briefly to discover further sensors.</div>
        <div class="form-actions"><button class="btn btn--primary" data-save data-i18n="common.save">Save</button></div>
      </div>
    </section>

    <section class="section" id="section-network">
      <p class="section-lead" data-i18n="network.lead">WiFi connection and IP settings of the receiver.</p>
      <div class="panel">
        <div class="panel__header"><h2 data-i18n="network.status">Status</h2><span class="badge" id="net-badge">&ndash;</span></div>
        <div class="grid" style="grid-template-columns:repeat(auto-fill,minmax(180px,1fr))">
          <div><div class="card__subtitle">SSID</div><div class="mono" id="n-ssid">&ndash;</div></div>
          <div><div class="card__subtitle">RSSI</div><div class="mono" id="n-rssi">&ndash;</div></div>
          <div><div class="card__subtitle" data-i18n="network.ip">IP address</div><div class="mono" id="n-ip">&ndash;</div></div>
          <div><div class="card__subtitle" data-i18n="network.gateway">Gateway</div><div class="mono" id="n-gw">&ndash;</div></div>
          <div><div class="card__subtitle" data-i18n="network.subnet">Subnet mask</div><div class="mono" id="n-mask">&ndash;</div></div>
          <div><div class="card__subtitle">DNS</div><div class="mono" id="n-dns">&ndash;</div></div>
          <div><div class="card__subtitle">MAC</div><div class="mono" id="n-mac">&ndash;</div></div>
          <div><div class="card__subtitle">Hostname</div><div class="mono" id="n-host">&ndash;</div></div>
        </div>
      </div>
      <div class="panel">
        <div class="panel__header"><h2 data-i18n="network.wifi">WiFi network</h2><button class="btn btn--sm" id="scan-btn" data-i18n="network.scan">Scan</button></div>
        <div id="scan-results" class="scan-results" style="display:none"></div>
        <div class="field"><label for="f-ssid">SSID</label><input type="text" id="f-ssid" maxlength="32" autocomplete="off"></div>
        <div class="field"><label for="f-wifipw" data-i18n="network.wifipw">WiFi password (empty = unchanged)</label><input type="password" id="f-wifipw" maxlength="63" autocomplete="new-password"><div class="field-hint" data-i18n="network.wifipw_hint">If you change the SSID and leave the password empty, an open network is assumed.</div></div>
        <div class="field"><label for="f-hostname">Hostname</label><input type="text" id="f-hostname" maxlength="31"></div>
        <div class="checkbox-row"><input type="checkbox" id="f-dhcp"><label for="f-dhcp" data-i18n="network.dhcp">Obtain IP address automatically (DHCP)</label></div>
        <div class="field--row">
          <div class="field"><label for="f-ip" data-i18n="network.ip">IP address</label><input type="text" id="f-ip" maxlength="15"></div>
          <div class="field"><label for="f-gw" data-i18n="network.gateway">Gateway</label><input type="text" id="f-gw" maxlength="15"></div>
        </div>
        <div class="field--row">
          <div class="field"><label for="f-mask" data-i18n="network.subnet">Subnet mask</label><input type="text" id="f-mask" maxlength="15"></div>
          <div class="field"><label for="f-dns">DNS</label><input type="text" id="f-dns" maxlength="15"></div>
        </div>
        <div class="field"><label for="f-netpw" data-i18n="network.authpw">Device password (user: admin)</label><input type="password" id="f-netpw" autocomplete="current-password"><div class="field-hint" data-i18n="network.apply_hint">The device restarts after saving. A wrong WiFi password keeps the previous network. With a new network or static IP the page may be unreachable; the new IP is shown on the OLED.</div></div>
        <div class="form-actions"><button class="btn btn--primary" id="net-save" data-i18n="network.save">Save &amp; restart</button></div>
      </div>
    </section>

    <section class="section" id="section-mqtt">
      <p class="section-lead" data-i18n="mqtt.lead">Publishes every received packet as JSON to &lt;base topic&gt;/data/&lt;sensor ID&gt;.</p>
      <div class="panel">
        <div class="panel__header"><h2>MQTT</h2><span class="badge" id="mqtt-badge">&ndash;</span></div>
        <div class="checkbox-row"><input type="checkbox" id="f-mqtt"><label for="f-mqtt" data-i18n="mqtt.enable">Enable MQTT</label></div>
        <div class="field--row">
          <div class="field"><label for="f-host" data-i18n="mqtt.host">Host</label><input type="text" id="f-host" maxlength="63"></div>
          <div class="field"><label for="f-port" data-i18n="mqtt.port">Port</label><input type="number" id="f-port" min="1" max="65535"></div>
        </div>
        <div class="field--row">
          <div class="field"><label for="f-user" data-i18n="mqtt.user">User</label><input type="text" id="f-user" maxlength="31" autocomplete="off"></div>
          <div class="field"><label for="f-pass" data-i18n="mqtt.pass">Password (empty = unchanged)</label><input type="password" id="f-pass" maxlength="31" autocomplete="new-password"></div>
        </div>
        <div class="field"><label for="f-topic" data-i18n="mqtt.topic">Base topic</label><input type="text" id="f-topic" maxlength="63"></div>
        <div class="form-actions"><button class="btn btn--primary" data-save data-i18n="common.save">Save</button></div>
      </div>
    </section>
    <section class="section" id="section-console">
      <p class="section-lead" data-i18n="console.lead">Live view of the serial output, including every received radio packet.</p>
      <div class="panel">
        <div class="panel__header">
          <h2 data-i18n="console.heading">Serial console</h2>
          <div style="display:flex;gap:.5rem;flex-wrap:wrap">
            <button class="btn btn--sm" id="con-pause" data-i18n="console.pause">Pause</button>
            <button class="btn btn--sm" id="con-clear" data-i18n="console.clear">Clear</button>
            <button class="btn btn--sm" id="con-copy" data-i18n="console.copy">Copy</button>
          </div>
        </div>
        <pre id="con-out" class="console"></pre>
        <div class="checkbox-row" style="margin-bottom:0"><input type="checkbox" id="con-follow" checked><label for="con-follow" data-i18n="console.follow">Scroll to newest line</label></div>
      </div>
    </section>

    <section class="section" id="section-system">
      <p class="section-lead" data-i18n="system.lead">Firmware update over the air. Upload the compiled .bin file (Arduino IDE: Sketch &rarr; Export Compiled Binary, use the file without "bootloader" or "partitions").</p>
      <div class="panel">
        <div class="panel__header"><h2 data-i18n="system.fw_heading">Firmware</h2><span class="badge mono" id="fw-build">&ndash;</span></div>
        <div class="field"><label for="f-fwfile" data-i18n="system.file">Firmware file (.bin)</label><input type="file" id="f-fwfile" accept=".bin"></div>
        <div class="field"><label for="f-otapw" data-i18n="system.ota_login">OTA password (user: admin)</label><input type="password" id="f-otapw" autocomplete="current-password"></div>
        <progress id="ota-progress" value="0" max="100" style="width:100%;display:none"></progress>
        <div class="form-actions"><button class="btn btn--primary" id="ota-btn" data-i18n="system.upload">Upload &amp; restart</button></div>
      </div>
      <div class="panel">
        <div class="panel__header"><h2 data-i18n="system.restart_heading">Restart device</h2></div>
        <p class="panel__desc" data-i18n="system.restart_desc">Restarts the receiver. The web UI is unreachable for about 15 seconds.</p>
        <div class="field"><label for="f-restartpw" data-i18n="system.ota_login">OTA password (user: admin)</label><input type="password" id="f-restartpw" autocomplete="current-password"></div>
        <div class="form-actions"><button class="btn btn--danger" id="restart-btn" data-i18n="system.restart_btn">Restart</button></div>
      </div>
      <div class="panel">
        <div class="panel__header"><h2 data-i18n="system.pw_heading">Change OTA password</h2></div>
        <div class="field"><label for="f-newotapw" data-i18n="system.new_pw">New OTA password (empty = unchanged)</label><input type="password" id="f-newotapw" maxlength="31" autocomplete="new-password"><div class="field-hint" data-i18n="system.pw_hint">Default is "bresser". Also used for network uploads from the Arduino IDE (port BresserHeltec).</div></div>
        <div class="form-actions"><button class="btn btn--primary" data-save data-i18n="common.save">Save</button></div>
      </div>
    </section>
  </main>
</div>
<div class="toasts" id="toasts"></div>
<script src="/app.js"></script>
</body>
</html>
)HTML";

const char WEB_JS[] PROGMEM = R"JS("use strict";
const I18N = {
  en: {
    "app.name": "Bresser Weather Receiver", "statusbar.wifi": "WiFi", "statusbar.sensor": "Sensor",
    "nav.dashboard": "Weather", "nav.sensor": "Sensor", "nav.mqtt": "MQTT",
    "footer.tagline": "Heltec WiFi LoRa 32 V2 · Bresser 5-in-1",
    "common.save": "Save", "common.saved": "Saved", "common.save_error": "Save failed: {msg}",
    "common.offline": "offline", "common.live": "Live", "common.no_data": "No data yet", "common.lost": "Receiver unreachable",
    "dashboard.lead": "Live values from the Bresser 5-in-1 weather station.",
    "d.temp": "Temperature", "d.hum": "Humidity", "d.dew": "Dew point", "d.wind": "Wind", "d.gust": "Gust",
    "d.rain": "Rain total", "d.rain1h": "Rain 1 h", "d.rain24h": "Rain 24 h", "d.rain7d": "Rain 7 d",
    "d.battery": "Battery", "d.dir": "Wind direction", "d.batt_ok": "OK", "d.batt_low": "LOW", "d.east": "E", "d.age": "updated {s} s ago",
    "sensor.lead": "If several weather stations are in range, you can restrict reception to one sensor ID.",
    "sensor.heading": "Sensor selection", "sensor.filter": "Use only the selected sensor ID", "sensor.id": "Sensor ID",
    "sensor.all": "All sensors / no selection", "sensor.none_seen": "No sensors detected yet. Wait for reception or briefly disable the filter to find other stations.",
    "sensor.seen": "Detected: {list}", "sensor.saved_unseen": "{id} (saved, not seen again yet)",
    "sensor.hint": "While the filter is active, other stations are blocked in the receiver. Disable it briefly to discover further sensors.",
    "mqtt.lead": "Publishes every received packet as JSON to <base topic>/data/<sensor ID>.",
    "mqtt.enable": "Enable MQTT", "mqtt.host": "Host", "mqtt.port": "Port", "mqtt.user": "User",
    "mqtt.pass": "Password (empty = unchanged)", "mqtt.topic": "Base topic",
    "nav.system": "System",
    "nav.console": "Console", "console.lead": "Live view of the serial output, including every received radio packet.",
    "console.heading": "Serial console", "console.pause": "Pause", "console.resume": "Resume", "console.clear": "Clear", "console.copy": "Copy",
    "console.copied": "Copied to clipboard", "console.follow": "Scroll to newest line",
    "nav.network": "Network",
    "network.lead": "WiFi connection and IP settings of the receiver.", "network.status": "Status", "network.ip": "IP address",
    "network.gateway": "Gateway", "network.subnet": "Subnet mask", "network.wifi": "WiFi network", "network.scan": "Scan",
    "network.scanning": "Scanning…", "network.no_networks": "No networks found", "network.connected": "Connected", "network.disconnected": "Not connected",
    "network.wifipw": "WiFi password (empty = unchanged)", "network.wifipw_hint": "If you change the SSID and leave the password empty, an open network is assumed.",
    "network.dhcp": "Obtain IP address automatically (DHCP)", "network.authpw": "Device password (user: admin)",
    "network.apply_hint": "The device restarts after saving. A wrong WiFi password keeps the previous network. With a new network or static IP the page may be unreachable; the new IP is shown on the OLED.",
    "network.save": "Save & restart", "network.saved": "Saved, device restarting…", "network.save_error": "Saving failed: {msg}", "network.unauthorized": "wrong device password",
    "system.lead": "Firmware update over the air. Upload the compiled .bin file (Arduino IDE: Sketch → Export Compiled Binary, use the file without \"bootloader\" or \"partitions\").",
    "system.fw_heading": "Firmware", "system.file": "Firmware file (.bin)", "system.ota_login": "OTA password (user: admin)",
    "system.upload": "Upload & restart", "system.pw_heading": "Change OTA password", "system.new_pw": "New OTA password (empty = unchanged)",
    "system.pw_hint": "Default is \"bresser\". Also used for network uploads from the Arduino IDE (port BresserHeltec).",
    "system.restart_heading": "Restart device", "system.restart_desc": "Restarts the receiver. The web UI is unreachable for about 15 seconds.",
    "system.restart_btn": "Restart", "system.restart_confirm": "Restart the device now?", "system.restarting": "Device restarting…",
    "system.no_file": "Choose a .bin file first", "system.done": "Update installed, device restarting…",
    "system.failed": "Update failed: {msg}", "system.unauthorized": "wrong OTA password"
  },
  de: {
    "app.name": "Bresser Wetterempfänger", "statusbar.wifi": "WLAN", "statusbar.sensor": "Sensor",
    "nav.dashboard": "Wetter", "nav.sensor": "Sensor", "nav.mqtt": "MQTT",
    "footer.tagline": "Heltec WiFi LoRa 32 V2 · Bresser 5-in-1",
    "common.save": "Speichern", "common.saved": "Gespeichert", "common.save_error": "Speichern fehlgeschlagen: {msg}",
    "common.offline": "offline", "common.live": "Live", "common.no_data": "Noch keine Daten", "common.lost": "Empfänger nicht erreichbar",
    "dashboard.lead": "Live-Werte der Bresser-5-in-1-Wetterstation.",
    "d.temp": "Temperatur", "d.hum": "Luftfeuchte", "d.dew": "Taupunkt", "d.wind": "Wind", "d.gust": "Böe",
    "d.rain": "Regen gesamt", "d.rain1h": "Regen 1 h", "d.rain24h": "Regen 24 h", "d.rain7d": "Regen 7 d",
    "d.battery": "Batterie", "d.dir": "Windrichtung", "d.batt_ok": "OK", "d.batt_low": "LEER", "d.east": "O", "d.age": "vor {s} s aktualisiert",
    "sensor.lead": "Wenn mehrere Wetterstationen in Reichweite sind, kann der Empfang auf eine Sensor-ID beschränkt werden.",
    "sensor.heading": "Sensor-Auswahl", "sensor.filter": "Nur ausgewählte Sensor-ID verwenden", "sensor.id": "Sensor-ID",
    "sensor.all": "Alle Sensoren / keine Auswahl", "sensor.none_seen": "Noch keine Sensoren erkannt. Empfang abwarten oder den Filter kurz deaktivieren, um weitere Stationen zu finden.",
    "sensor.seen": "Erkannt: {list}", "sensor.saved_unseen": "{id} (gespeichert, noch nicht wieder gesehen)",
    "sensor.hint": "Solange der Filter aktiv ist, werden fremde Stationen schon im Empfänger blockiert. Zum Suchen weiterer Sensoren den Filter kurz deaktivieren.",
    "mqtt.lead": "Veröffentlicht jedes empfangene Paket als JSON unter <Base Topic>/data/<Sensor-ID>.",
    "mqtt.enable": "MQTT aktivieren", "mqtt.host": "Host", "mqtt.port": "Port", "mqtt.user": "Benutzer",
    "mqtt.pass": "Passwort (leer = unverändert)", "mqtt.topic": "Base Topic",
    "nav.system": "System",
    "nav.console": "Konsole", "console.lead": "Live-Ansicht der seriellen Ausgabe, einschließlich jedes empfangenen Funkpakets.",
    "console.heading": "Serielle Konsole", "console.pause": "Pause", "console.resume": "Weiter", "console.clear": "Leeren", "console.copy": "Kopieren",
    "console.copied": "In die Zwischenablage kopiert", "console.follow": "Zur neuesten Zeile scrollen",
    "nav.network": "Netzwerk",
    "network.lead": "WLAN-Verbindung und IP-Einstellungen des Empfängers.", "network.status": "Status", "network.ip": "IP-Adresse",
    "network.gateway": "Gateway", "network.subnet": "Subnetzmaske", "network.wifi": "WLAN-Netzwerk", "network.scan": "Suchen",
    "network.scanning": "Suche läuft…", "network.no_networks": "Keine Netzwerke gefunden", "network.connected": "Verbunden", "network.disconnected": "Nicht verbunden",
    "network.wifipw": "WLAN-Passwort (leer = unverändert)", "network.wifipw_hint": "Wird die SSID geändert und das Passwort leer gelassen, wird ein offenes Netz angenommen.",
    "network.dhcp": "IP-Adresse automatisch beziehen (DHCP)", "network.authpw": "Geräte-Passwort (Benutzer: admin)",
    "network.apply_hint": "Nach dem Speichern startet das Gerät neu. Bei falschem WLAN-Passwort bleibt das bisherige Netz erhalten. Bei neuem Netz oder fester IP ist die Seite evtl. nicht mehr erreichbar; die neue IP steht auf dem OLED.",
    "network.save": "Speichern & neu starten", "network.saved": "Gespeichert, Gerät startet neu…", "network.save_error": "Speichern fehlgeschlagen: {msg}", "network.unauthorized": "falsches Geräte-Passwort",
    "system.lead": "Firmware-Update über WLAN. Die kompilierte .bin-Datei hochladen (Arduino IDE: Sketch → Kompilierte Binärdatei exportieren, die Datei ohne „bootloader“ oder „partitions“ verwenden).",
    "system.fw_heading": "Firmware", "system.file": "Firmware-Datei (.bin)", "system.ota_login": "OTA-Passwort (Benutzer: admin)",
    "system.upload": "Hochladen & neu starten", "system.pw_heading": "OTA-Passwort ändern", "system.new_pw": "Neues OTA-Passwort (leer = unverändert)",
    "system.pw_hint": "Standard ist „bresser“. Gilt auch für Netzwerk-Uploads aus der Arduino IDE (Port BresserHeltec).",
    "system.restart_heading": "Gerät neu starten", "system.restart_desc": "Startet den Empfänger neu. Die Web-UI ist danach etwa 15 Sekunden nicht erreichbar.",
    "system.restart_btn": "Neu starten", "system.restart_confirm": "Gerät jetzt neu starten?", "system.restarting": "Gerät startet neu…",
    "system.no_file": "Bitte zuerst eine .bin-Datei wählen", "system.done": "Update installiert, Gerät startet neu…",
    "system.failed": "Update fehlgeschlagen: {msg}", "system.unauthorized": "falsches OTA-Passwort"
  }
};
const LANG_KEY = "bresser.lang", ROUTES = ["dashboard", "sensor", "network", "mqtt", "console", "system"];
let lang = "en", route = "dashboard", lastData = null, formsLoaded = false;
try { const s = localStorage.getItem(LANG_KEY); if (s === "de" || s === "en") lang = s; } catch (e) {}
const $ = id => document.getElementById(id);
const t = (k, v) => { let s = (I18N[lang][k] ?? I18N.en[k] ?? k); if (v) for (const x in v) s = s.replace("{" + x + "}", v[x]); return s; };
const num = (n, d = 1) => Number(n).toLocaleString(lang, { minimumFractionDigits: d, maximumFractionDigits: d });

function toast(msg, kind) {
  const el = document.createElement("div"); el.className = "toast " + (kind || ""); el.textContent = msg;
  $("toasts").appendChild(el); setTimeout(() => el.remove(), 3500);
}
function applyI18n() {
  document.documentElement.lang = lang; document.title = t("app.name");
  document.querySelectorAll("[data-i18n]").forEach(e => { e.textContent = t(e.dataset.i18n); });
  $("rose-e").textContent = t("d.east"); $("lang-select").value = lang;
  $("page-title").textContent = t("nav." + route);
  if (lastData) render(lastData); loadSensors();
}
function setRoute(r) {
  if (!ROUTES.includes(r)) r = "dashboard"; route = r;
  ROUTES.forEach(x => $("section-" + x).classList.toggle("active", x === r));
  document.querySelectorAll(".nav__item").forEach(b => b.classList.toggle("active", b.dataset.route === r));
  $("page-title").textContent = t("nav." + r); $("sidebar").classList.remove("open");
  if (location.hash !== "#" + r) history.replaceState(null, "", "#" + r);
}
function dot(id, cls) { $(id).className = "statusbar__dot" + (cls ? " " + cls : ""); }

function render(d) {
  lastData = d;
  $("sb-wifi").textContent = d.wifi_connected ? d.ip : t("common.offline"); dot("sb-wifi-dot", d.wifi_connected ? "ok" : "warn");
  $("footer-ip").textContent = d.wifi_connected ? d.ip : "–";
  $("sb-mqtt").textContent = d.mqtt_status; dot("sb-mqtt-dot", d.mqtt_status === "connected" ? "ok" : (d.mqtt_status === "disabled" ? "" : "warn"));
  const mb = $("mqtt-badge"); mb.textContent = d.mqtt_status;
  mb.className = "badge" + (d.mqtt_status === "connected" ? " ok" : (d.mqtt_status === "disabled" ? "" : " warn"));
  $("sb-sensor").textContent = d.valid ? d.id_hex.toUpperCase().padStart(8, "0") : "–";
  const fresh = d.valid && d.age_s < 120; dot("sb-sensor-dot", d.valid ? (fresh ? "ok" : "warn") : "");
  const lb = $("live-badge");
  lb.textContent = d.valid ? (fresh ? t("common.live") : t("d.age", { s: d.age_s })) : t("common.no_data");
  lb.className = "badge" + (d.valid ? (fresh ? " ok" : " warn") : "");
  const v = (id, val, dg) => { $(id).textContent = d.valid ? num(val, dg) : "–"; };
  v("v-temp", d.temp_c); v("v-hum", d.humidity, 0); v("v-dew", d.dew_point_c); v("v-wind", d.wind_speed_ms); v("v-gust", d.wind_gust_ms);
  v("v-rain", d.rain_mm); v("v-rain1h", d.rain_hour_mm); v("v-rain24h", d.rain_day_mm); v("v-rain7d", d.rain_week_mm);
  $("v-kmh").textContent = d.valid ? num(d.wind_speed_ms * 3.6) + " km/h" : "–";
  $("v-dirtext").textContent = d.valid ? d.wind_dir_text : "–";
  $("v-dirdeg").textContent = d.valid ? num(d.wind_dir_deg, 0) + "°" : "–";
  $("wind-arrow").setAttribute("transform", "rotate(" + Number(d.wind_dir_deg || 0) + " 52 52)");
  const bt = $("v-batt");
  bt.textContent = d.valid ? (d.battery_ok ? t("d.batt_ok") : t("d.batt_low")) : "–";
  bt.style.color = d.valid ? (d.battery_ok ? "var(--ok)" : "var(--danger)") : "";
  $("v-rssi").textContent = "RSSI " + (d.valid ? num(d.rssi) + " dBm" : "–");
}
async function poll() {
  try { const r = await fetch("/data", { cache: "no-store" }); render(await r.json()); }
  catch (e) { const lb = $("live-badge"); lb.textContent = t("common.lost"); lb.className = "badge bad"; }
}
async function loadConfig() {
  try {
    const c = await (await fetch("/api/config", { cache: "no-store" })).json();
    $("f-filter").checked = c.sensor_filter_enabled; $("f-mqtt").checked = c.mqtt_enabled;
    $("f-host").value = c.mqtt_host; $("f-port").value = c.mqtt_port; $("f-user").value = c.mqtt_user; $("f-topic").value = c.base_topic;
    $("f-pass").value = ""; $("f-newotapw").value = ""; $("fw-build").textContent = c.fw_build; $("f-sensor").dataset.current = c.selected_sensor_id_hex || ""; formsLoaded = true;
  } catch (e) {}
  loadSensors();
}
async function loadSensors() {
  try {
    const d = await (await fetch("/sensors", { cache: "no-store" })).json();
    const sel = $("f-sensor"), hint = $("seen-hint");
    const cur = formsLoaded ? (sel.value || sel.dataset.current || "") : (sel.dataset.current || "");
    sel.innerHTML = "";
    const all = document.createElement("option"); all.value = ""; all.textContent = t("sensor.all"); sel.appendChild(all);
    let found = !cur;
    d.sensors.forEach(s => {
      const o = document.createElement("option"); o.value = s.id_hex;
      o.textContent = s.id_hex + "  " + num(s.rssi) + " dBm · " + s.age_s + " s"; if (s.id_hex === cur) found = true; sel.appendChild(o);
    });
    if (!found) { const o = document.createElement("option"); o.value = cur; o.textContent = t("sensor.saved_unseen", { id: cur }); sel.appendChild(o); }
    sel.value = cur; sel.dataset.current = cur;
    hint.textContent = d.sensors.length ? t("sensor.seen", { list: d.sensors.map(s => s.id_hex).join(", ") }) : t("sensor.none_seen");
  } catch (e) {}
}
async function save() {
  const body = new URLSearchParams();
  if ($("f-filter").checked) body.set("sensorFilter", "1");
  body.set("sensorId", $("f-sensor").value);
  if ($("f-mqtt").checked) body.set("enabled", "1");
  body.set("host", $("f-host").value); body.set("port", $("f-port").value); body.set("user", $("f-user").value);
  if ($("f-pass").value) body.set("pass", $("f-pass").value);
  body.set("topic", $("f-topic").value);
  if ($("f-newotapw").value) body.set("otaPass", $("f-newotapw").value);
  try {
    const r = await fetch("/save", { method: "POST", body }); const j = await r.json();
    if (!r.ok || !j.ok) throw new Error(j.error || r.status);
    toast(t("common.saved"), "success"); loadConfig(); poll();
  } catch (e) { toast(t("common.save_error", { msg: e.message }), "error"); }
}
let logPos = 0, logPaused = false, logLines = 0;
const LOG_MAX_LINES = 600;
function logClass(line) {
  if (line.includes("(weitere Station)")) return "l-other";
  if (/^\[\d+s\] Sensor [0-9A-F]{8} \|/.test(line)) return "l-sensor";
  if (/fail|Fehler|FEHLER|panic/i.test(line)) return "l-err";
  if (/ignoriert|kein Paket/.test(line)) return "l-warn";
  return "";
}
function appendLog(text) {
  const out = $("con-out");
  text.split("\n").forEach(line => {
    if (!line.trim()) return;
    const span = document.createElement("div"), c = logClass(line);
    if (c) span.className = c;
    span.textContent = line; out.appendChild(span); logLines++;
  });
  while (logLines > LOG_MAX_LINES) { out.removeChild(out.firstChild); logLines--; }
  if ($("con-follow").checked) out.scrollTop = out.scrollHeight;
}
async function pollLog() {
  if (route !== "console" || logPaused) return;
  try {
    const r = await fetch("/api/log?since=" + logPos, { cache: "no-store" });
    const next = Number(r.headers.get("X-Log-Next"));
    const text = await r.text();
    if (next < logPos) { $("con-out").textContent = ""; logLines = 0; }
    logPos = next; if (text) appendLog(text);
  } catch (e) {}
}
let netFormDirty = false;
function syncDhcp() { ["f-ip", "f-gw", "f-mask", "f-dns"].forEach(id => { $(id).disabled = $("f-dhcp").checked; }); }
async function loadNetwork(fillForm) {
  try {
    const n = await (await fetch("/api/network", { cache: "no-store" })).json();
    $("n-ssid").textContent = n.ssid || "–"; $("n-rssi").textContent = n.connected ? n.rssi + " dBm" : "–";
    $("n-ip").textContent = n.ip || "–"; $("n-gw").textContent = n.gateway || "–"; $("n-mask").textContent = n.subnet || "–";
    $("n-dns").textContent = n.dns || "–"; $("n-mac").textContent = n.mac; $("n-host").textContent = n.hostname;
    const b = $("net-badge"); b.textContent = n.connected ? t("network.connected") : t("network.disconnected");
    b.className = "badge" + (n.connected ? " ok" : " warn");
    if (fillForm && !netFormDirty) {
      $("f-ssid").value = n.ssid || ""; $("f-wifipw").value = ""; $("f-hostname").value = n.hostname;
      $("f-dhcp").checked = n.dhcp;
      $("f-ip").value = n.static_ip || n.ip; $("f-gw").value = n.static_gateway || n.gateway;
      $("f-mask").value = n.static_subnet || n.subnet; $("f-dns").value = n.static_dns || n.dns; syncDhcp();
    }
  } catch (e) {}
}
async function scanNetworks() {
  const box = $("scan-results"), btn = $("scan-btn");
  box.style.display = "block"; box.innerHTML = ""; box.textContent = t("network.scanning"); btn.disabled = true;
  try {
    await fetch("/api/network/scan", { method: "POST" });
    for (let i = 0; i < 20; i++) {
      await new Promise(r => setTimeout(r, 1000));
      const d = await (await fetch("/api/network/scan", { cache: "no-store" })).json();
      if (d.running) continue;
      box.textContent = "";
      if (!d.networks.length) { box.textContent = t("network.no_networks"); break; }
      d.networks.sort((a, b) => b.rssi - a.rssi).forEach(s => {
        const row = document.createElement("div"); row.className = "scan-results__item";
        const name = document.createElement("span"); name.textContent = s.ssid;
        const meta = document.createElement("span"); meta.className = "scan-results__meta"; meta.textContent = (s.secure ? "🔒 " : "") + s.rssi + " dBm";
        row.append(name, meta);
        row.addEventListener("click", () => { $("f-ssid").value = s.ssid; netFormDirty = true; $("f-wifipw").focus(); });
        box.appendChild(row);
      });
      break;
    }
  } catch (e) { box.textContent = t("network.no_networks"); }
  btn.disabled = false;
}
async function saveNetwork() {
  const body = new URLSearchParams();
  body.set("ssid", $("f-ssid").value); body.set("wifiPass", $("f-wifipw").value); body.set("hostname", $("f-hostname").value);
  if ($("f-dhcp").checked) body.set("dhcp", "1");
  body.set("ip", $("f-ip").value); body.set("gateway", $("f-gw").value); body.set("subnet", $("f-mask").value); body.set("dns", $("f-dns").value);
  try {
    const r = await fetch("/api/network", { method: "POST", body, headers: { Authorization: "Basic " + btoa("admin:" + $("f-netpw").value) } });
    const j = await r.json();
    if (!r.ok || !j.ok) throw new Error(r.status === 401 ? t("network.unauthorized") : (j.error || r.status));
    toast(t("network.saved"), "success"); netFormDirty = false; $("net-save").disabled = true;
    setTimeout(() => location.reload(), 20000);
  } catch (e) { toast(t("network.save_error", { msg: e.message }), "error"); }
}
function uploadFirmware() {
  const file = $("f-fwfile").files[0];
  if (!file) { toast(t("system.no_file"), "error"); return; }
  const fd = new FormData(); fd.append("firmware", file, file.name);
  const xhr = new XMLHttpRequest(), bar = $("ota-progress"), btn = $("ota-btn");
  xhr.open("POST", "/api/system/ota");
  xhr.setRequestHeader("Authorization", "Basic " + btoa("admin:" + $("f-otapw").value));
  xhr.upload.onprogress = e => { if (e.lengthComputable) bar.value = Math.round(e.loaded * 100 / e.total); };
  xhr.onload = () => {
    btn.disabled = false;
    if (xhr.status === 200) { toast(t("system.done"), "success"); setTimeout(() => location.reload(), 8000); }
    else toast(t("system.failed", { msg: xhr.status === 401 ? t("system.unauthorized") : xhr.status }), "error");
  };
  xhr.onerror = () => { btn.disabled = false; toast(t("system.failed", { msg: "network" }), "error"); };
  bar.value = 0; bar.style.display = "block"; btn.disabled = true; xhr.send(fd);
}
$("ota-btn").addEventListener("click", uploadFirmware);
$("restart-btn").addEventListener("click", async () => {
  if (!confirm(t("system.restart_confirm"))) return;
  try {
    const r = await fetch("/api/system/restart", { method: "POST", headers: { Authorization: "Basic " + btoa("admin:" + $("f-restartpw").value) } });
    if (!r.ok) throw new Error(r.status === 401 ? t("system.unauthorized") : r.status);
    toast(t("system.restarting"), "success"); setTimeout(() => location.reload(), 15000);
  } catch (e) { toast(t("system.failed", { msg: e.message }), "error"); }
});
$("con-pause").addEventListener("click", () => {
  logPaused = !logPaused; $("con-pause").textContent = t(logPaused ? "console.resume" : "console.pause"); if (!logPaused) pollLog();
});
$("con-clear").addEventListener("click", () => { $("con-out").textContent = ""; logLines = 0; });
$("con-copy").addEventListener("click", async () => {
  try { await navigator.clipboard.writeText($("con-out").innerText); toast(t("console.copied"), "success"); }
  catch (e) { const r = document.createRange(); r.selectNodeContents($("con-out")); const s = getSelection(); s.removeAllRanges(); s.addRange(r); }
});
$("scan-btn").addEventListener("click", scanNetworks);
$("net-save").addEventListener("click", saveNetwork);
$("f-dhcp").addEventListener("change", () => { netFormDirty = true; syncDhcp(); });
["f-ssid", "f-wifipw", "f-hostname", "f-ip", "f-gw", "f-mask", "f-dns"].forEach(id => $(id).addEventListener("input", () => { netFormDirty = true; }));
document.querySelectorAll(".nav__item").forEach(b => b.addEventListener("click", () => setRoute(b.dataset.route)));
document.querySelectorAll("[data-save]").forEach(b => b.addEventListener("click", save));
$("menu-btn").addEventListener("click", () => $("sidebar").classList.toggle("open"));
$("lang-select").addEventListener("change", e => { lang = e.target.value; try { localStorage.setItem(LANG_KEY, lang); } catch (x) {} applyI18n(); });
window.addEventListener("hashchange", () => setRoute(location.hash.slice(1)));
setRoute(location.hash.slice(1)); applyI18n(); loadConfig(); loadNetwork(true); poll();
setInterval(poll, 3000); setInterval(pollLog, 1500); setInterval(() => { if (route === "sensor") loadSensors(); if (route === "network") loadNetwork(false); }, 5000);
)JS";
