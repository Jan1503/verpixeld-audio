// WebServer.h — config page on socket 1, TCP 5000, served from core 1.
// Same shape as the panel: one HTML page, /status JSON, /cmd into the serial parser on core 0.
#pragma once
#include "hardware/clocks.h"

static const char WEB_PAGE[] = R"HTML(<!doctype html><html lang=en><head>
<meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>verpixeld</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%23080d14'/%3E%3Crect x='5' y='7' width='5' height='18' rx='1' fill='%233ee0ff'/%3E%3Crect x='13.5' y='7' width='5' height='18' rx='1' fill='%23e8eef6'/%3E%3Crect x='22' y='7' width='5' height='18' rx='1' fill='%237c5cff'/%3E%3C/svg%3E">
<style>
:root{--bg:#07090d;--card:#10151c;--line:#1d2733;--tx:#e8eef6;--mut:#8b97a8;--ac:#3ee0ff;--ac2:#7c5cff;--ok:#3ee0a0;--bad:#ff5b6e;--warn:#ffc14a}
*{box-sizing:border-box}body{margin:0;font:14px/1.45 system-ui,Segoe UI,sans-serif;color:var(--tx);background:radial-gradient(900px 420px at 8% -8%,rgba(62,224,255,.14),transparent 55%),radial-gradient(800px 380px at 110% 0,rgba(124,92,255,.12),transparent 50%),var(--bg)}
header{display:flex;justify-content:space-between;align-items:center;gap:16px;padding:18px 22px;border-bottom:1px solid var(--line);background:linear-gradient(90deg,#0c1520,#07090d);position:relative;overflow:hidden}
header:before{content:"";position:absolute;inset:0;background-image:repeating-linear-gradient(90deg,transparent,transparent 9px,rgba(62,224,255,.045) 9px,rgba(62,224,255,.045) 10px),repeating-linear-gradient(0deg,transparent,transparent 9px,rgba(62,224,255,.045) 9px,rgba(62,224,255,.045) 10px);pointer-events:none}
header:after{content:"";position:absolute;left:0;right:0;height:1px;background:linear-gradient(90deg,transparent,var(--ac),transparent);animation:scan 5s linear infinite;opacity:.45;pointer-events:none}
@keyframes scan{from{top:0}to{top:100%}}@keyframes pulse{50%{opacity:.35}}@keyframes spin{to{transform:rotate(360deg)}}
@media(prefers-reduced-motion:reduce){header:after,.pill.on i,.spin{animation:none}}
.brand{display:flex;gap:12px;align-items:center;position:relative}
.logo{display:flex;gap:3px}.logo i{width:6px;height:20px;border-radius:2px;background:var(--ac)}.logo i:nth-child(2){background:#e8eef6;height:14px;align-self:center}.logo i:nth-child(3){background:var(--ac2)}
h1{margin:0;font-size:18px;letter-spacing:.08em;text-transform:lowercase}.sub{color:var(--mut);font-size:12px;margin-top:2px}
.hdr{display:flex;gap:8px;position:relative;flex-wrap:wrap;justify-content:flex-end}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;background:#0d131b;border:1px solid var(--line);color:var(--mut);font-size:11px;letter-spacing:.08em;text-transform:uppercase}
.pill i{width:7px;height:7px;border-radius:50%;background:#4a5563}
.pill.on{color:#fff;border-color:rgba(255,91,110,.45)}.pill.on i{background:var(--bad);animation:pulse 1.3s ease-in-out infinite}
.wrap{max-width:920px;margin:0 auto;padding:16px;display:grid;gap:14px;grid-template-columns:1fr 1fr}
.metrics{grid-column:1/-1;display:grid;grid-template-columns:repeat(4,1fr);gap:12px}
.metric{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 16px}
.metric b{display:block;font-size:22px;font-variant-numeric:tabular-nums;letter-spacing:-.02em}
.metric span{color:var(--mut);font-size:11px;text-transform:uppercase;letter-spacing:.08em}
.metric.warn b{color:var(--warn)}.metric.bad b{color:var(--bad)}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px}
.card h2{margin:0 0 12px;font-size:11px;text-transform:uppercase;letter-spacing:.1em;color:var(--mut)}
.stat{display:flex;justify-content:space-between;gap:12px;padding:6px 0;border-bottom:1px solid #1a222c;font-size:13px}
.stat b{color:#fff;font-variant-numeric:tabular-nums;font-weight:600;text-align:right}
label{display:block;margin:10px 0 4px;color:var(--mut);font-size:12px}
input,select{width:100%;padding:8px 10px;background:#0a0e13;border:1px solid #2a3340;border-radius:8px;color:var(--tx)}
input[type=range]{padding:0;accent-color:var(--ac)}
.row{display:flex;gap:8px;align-items:end}.row>*{flex:1}
button{cursor:pointer;padding:8px 12px;border:0;border-radius:8px;background:linear-gradient(180deg,var(--ac),#2bb8d4);color:#04121f;font-weight:700;margin-top:8px}
button.sec{background:#1c2530;color:var(--tx);font-weight:600}
.btns{display:flex;flex-wrap:wrap;gap:8px}.btns button{flex:1;margin:0}
.full{grid-column:1/-1}small{color:var(--mut)}
.drop{border:1px dashed #2a3340;border-radius:12px;padding:20px;text-align:center;cursor:pointer;color:var(--mut)}
.drop.hot{border-color:var(--ac);color:var(--ac)}
.bar{display:none;height:4px;background:#1b2430;border-radius:4px;overflow:hidden;margin-top:10px}
.bar.on{display:block}#pbar{height:100%;width:0;background:linear-gradient(90deg,var(--ac),var(--ac2));transition:width .15s}
#ov{display:none;position:fixed;inset:0;background:rgba(4,7,10,.72);z-index:9;place-items:center}#ov.on{display:grid}
.ovc{display:flex;flex-direction:column;align-items:center;gap:12px;font-weight:700}
.spin{width:28px;height:28px;border:3px solid #243044;border-top-color:var(--ac);border-radius:50%;animation:spin .8s linear infinite}
@media(max-width:720px){.wrap,.metrics{grid-template-columns:1fr 1fr}header{flex-direction:column;align-items:flex-start}}
</style></head><body>
<div id=ov><div class=ovc><div class=spin></div><div id=ovt>Working…</div></div></div>
<header>
  <div class=brand><div class=logo><i></i><i></i><i></i></div><div><h1>verpixeld</h1><div class=sub id=sub>connecting…</div></div></div>
  <div class=hdr><span class=pill id=live><i></i>idle</span><span class=pill id=fw>—</span></div>
</header>
<div class=wrap>
  <div class=metrics>
    <div class=metric><b id=mVol>—</b><span>volume</span></div>
    <div class=metric id=cFill><b id=mFill>—</b><span>buffer</span></div>
    <div class=metric id=cUnd><b id=mUnd>—</b><span>underrun</span></div>
    <div class=metric id=cLink><b id=mLink>—</b><span>link</span></div>
  </div>
  <div class=card><h2>Status</h2><div id=st>…</div></div>
  <div class=card><h2>Playback</h2>
    <label>Volume <small id=vv>live</small></label>
    <input id=vol type=range min=0 max=100 value=70 oninput="$('vv').textContent=this.value" onchange="go('vol '+v('vol'))">
    <label>Test <small>1 kHz, headphone and speaker</small></label>
    <div class=btns>
      <button class=sec onclick="go('tone')">Tone</button>
      <button class=sec onclick="go('stop')">Stop</button>
      <button class=sec onclick="go('mute')">Mute</button>
    </div>
  </div>
  <div class=card><h2>Display SPI</h2>
    <label>Clock <small id=spiNow>…</small></label>
    <select id=spi onchange="go('spi '+this.value)"></select>
    <small>The list shows the clocks the board can really make. A reboot returns to the fastest. If the picture breaks, pick a lower clock. The page stays up either way.</small>
  </div>
  <div class="card full"><h2>Network <small>saves + reboots</small></h2>
    <div class=row><label><input type=radio name=nm value=dhcp checked> DHCP</label><label><input type=radio name=nm value=static> Static</label></div>
    <div class=row><div><label>IP</label><input id=ip></div><div><label>Mask</label><input id=mask></div><div><label>Gateway</label><input id=gw></div></div>
    <button onclick="applyNet()">Apply network</button>
  </div>
  <div class="card full"><h2>Firmware update</h2>
    <div id=drop class=drop>Drop the firmware <b>.ino.bin</b> here, or click to choose<input id=fwf type=file accept=".bin" style="display:none"></div>
    <div class=bar id=bar><div id=pbar></div></div>
    <div id=fwmsg style="margin-top:8px"></div>
  </div>
  <div class="card full"><h2>Config</h2>
    <div class=btns>
      <button onclick="go('save')">Save to flash</button>
      <button class=sec onclick="if(confirm('Reboot the board?')){overlay(true,'Rebooting…');go('reboot')}">Reboot</button>
    </div>
  </div>
</div>
<script>
const $=id=>document.getElementById(id),v=id=>$(id).value;
const cmd=c=>fetch('/cmd?c='+encodeURIComponent(c));
const go=c=>cmd(c).then(()=>setTimeout(refresh,180));
function overlay(on,t){$('ov').className=on?'on':'';if(t)$('ovt').textContent=t}
function row(k,val){return `<div class=stat><span>${k}</span><b>${val}</b></div>`}
let filled=false,uploading=false,timer=null;
function arm(){if(timer||document.hidden||uploading)return;timer=setInterval(refresh,2000)}
function disarm(){if(timer){clearInterval(timer);timer=null}}
document.addEventListener('visibilitychange',()=>{if(document.hidden)disarm();else{refresh();arm()}});
function refresh(){if(uploading||document.hidden)return;fetch('/status').then(r=>r.json()).then(paint).catch(()=>{})}
function paint(s){
  const live=!!s.tone||!!s.live;
  $('live').className='pill'+(live?' on':'');
  $('live').innerHTML='<i></i>'+(s.tone?'tone':(s.mute?'mute':(live?'play':'idle')));
  $('fw').textContent='fw '+s.ver;
  $('sub').textContent=(s.host||'audio')+' · '+(s.host||'')+'.local · '+(s.ip||'');
  document.title=(s.host||'verpixeld')+' · verpixeld';
  $('mVol').textContent=s.mute?'mute':s.vol;
  const ms=Math.round((s.fill||0)*1000/48000);
  $('mFill').textContent=ms+' ms';
  $('cFill').className='metric'+(ms<15&&live?' warn':'');
  $('mUnd').textContent=s.underrun;
  $('cUnd').className='metric'+(s.underrun>0?' warn':'');
  $('mLink').textContent=s.link?(s.spd+'M'):'down';
  $('cLink').className='metric'+(s.link?'':' bad');
  const link=s.link?('up · '+s.spd+' Mbit/s · '+(s.dpx?'full':'half')+' duplex'):'down';
  const state=s.tone?'tone':(s.mute?'mute':(live?'play':'idle'));
  $('st').innerHTML=row('Host',(s.host||'')+'.local')+row('MAC',s.mac||'—')+row('Link',link)+row('IP',s.ip)+row('Mask',s.mask)+row('Gateway',s.gw)+row('Addressing',s.dhcp?'DHCP':'Static')+row('Codec',s.codec==2?'I2S DAC (MAX98357A/PCM5102)':s.codec?'WM8960':'missing')+row('State',state)+row('SPI',(s.spik?Math.round(s.spik/10)/100:'?')+' MHz')+row('Title',s.title||'—')+row('Overrun',s.overrun)+row('Uptime',s.up+' s')+row('Audio','UDP '+s.udp)+row('Web','port '+s.web);
  if($('spi')&&s.peri&&!$('spi').options.length){for(let n=2;n<=24&&$('spi').options.length<6;n+=2){const k=s.peri*1000/n;if(k>62500||k<8000)continue;const o=document.createElement('option');o.value=Math.ceil(k/1000);o.dataset.k=Math.round(k);o.textContent=(Math.round(k/10)/100)+' MHz';$('spi').appendChild(o)}}
  if($('spi')&&document.activeElement!==$('spi')&&s.spik){for(const o of $('spi').options)if(Math.abs(o.dataset.k-s.spik)<2)o.selected=true;$('spiNow').textContent=(Math.round(s.spik/10)/100)+' MHz'}
  if(!filled){
    $('vol').value=s.vol;$('vv').textContent=s.vol;
    $('ip').value=s.ip;$('mask').value=s.mask;$('gw').value=s.gw;
    document.querySelector('input[name=nm][value="'+(s.dhcp?'dhcp':'static')+'"]').checked=true;
    filled=true;
  }else if(document.activeElement!==$('vol')){$('vol').value=s.vol;$('vv').textContent=s.mute?'mute':s.vol}
}
function applyNet(){
  const dhcp=document.querySelector('input[name=nm]:checked').value==='dhcp';
  if(!confirm('Save network settings and reboot?'))return;
  overlay(true,'Saving and rebooting…');
  cmd(dhcp?'applynet dhcp':'applynet static '+v('ip')+' '+v('mask')+' '+v('gw')).catch(()=>{}).finally(()=>setTimeout(()=>location.reload(),8000));
}
function fwUpload(file){
  if(!file.name.toLowerCase().endsWith('.bin')){$('fwmsg').textContent='Choose the .ino.bin (raw firmware), not the .uf2.';return}
  uploading=true;disarm();$('bar').classList.add('on');
  $('fwmsg').textContent='Uploading '+file.name+'… do not power off.';
  const x=new XMLHttpRequest();
  x.upload.onprogress=e=>{if(e.lengthComputable){$('pbar').style.width=(e.loaded/e.total*100)+'%';$('fwmsg').textContent='Uploading '+Math.round(e.loaded/e.total*100)+'%';}};
  x.onload=()=>{$('fwmsg').textContent='Board: '+x.responseText+' — reconnecting…';setTimeout(()=>location.reload(),9000)};
  x.onerror=()=>{$('fwmsg').textContent='Upload sent; board rebooting — reconnecting…';setTimeout(()=>location.reload(),9000)};
  x.open('POST','/update');x.send(file);
}
const drop=$('drop'),fwf=$('fwf');
drop.onclick=()=>fwf.click();
drop.ondragover=e=>{e.preventDefault();drop.classList.add('hot')};
drop.ondragleave=()=>drop.classList.remove('hot');
drop.ondrop=e=>{e.preventDefault();drop.classList.remove('hot');if(e.dataTransfer.files[0])fwUpload(e.dataTransfer.files[0])};
fwf.onchange=()=>{if(fwf.files[0])fwUpload(fwf.files[0])};
refresh();arm();
</script></body></html>)HTML";

static volatile char g_webCmd[96];
static volatile bool g_webCmdPending = false;

static int web_hex(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }

static void web_url_decode(char *dst, const char *src, int max)
{
    int i = 0;
    while (*src && i < max - 1) {
        char c = *src++;
        if (c == '&') break;
        if (c == '+') c = ' ';
        else if (c == '%' && src[0] && src[1]) {
            c = (char)((web_hex(src[0]) << 4) | web_hex(src[1]));
            src += 2;
        }
        dst[i++] = c;
    }
    dst[i] = 0;
}

static char     g_webHdr[160];
static uint16_t g_webHdrLen = 0, g_webHdrOff = 0;
static const uint8_t *g_webBody = nullptr;
static uint32_t g_webBodyLen = 0, g_webBodyOff = 0;
static uint8_t  g_webPhase = 0;

static void web_queue(const char *status, const char *ctype, const char *body, uint32_t blen)
{
    g_webHdrLen = (uint16_t)snprintf(g_webHdr, sizeof(g_webHdr),
        "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
        status, ctype, (unsigned long)blen);
    g_webHdrOff = 0;
    g_webBody = (const uint8_t *)body;
    g_webBodyLen = blen;
    g_webBodyOff = 0;
    g_webPhase = 1;
}

static void web_abort_send() { g_webPhase = 0; g_webBody = nullptr; }

static bool web_send_tick()
{
    const uint32_t budget = 1024;
    uint32_t used = 0;
    while (g_webPhase && used < budget) {
        const uint8_t *src;
        uint32_t left;
        if (g_webPhase == 1) {
            src = (const uint8_t *)g_webHdr + g_webHdrOff;
            left = (uint32_t)g_webHdrLen - g_webHdrOff;
        } else {
            src = g_webBody + g_webBodyOff;
            left = g_webBodyLen - g_webBodyOff;
        }
        if (left == 0) {
            g_webPhase = (g_webPhase == 1 && g_webBodyLen) ? 2 : 0;
            continue;
        }
        uint16_t fsr = w6_rd16(BSB_S1_REG, 0x0204);
        if (fsr == 0) return false;
        uint32_t chunk = left;
        if (chunk > fsr) chunk = fsr;
        if (chunk > budget - used) chunk = budget - used;
        if (chunk == 0) return false;
        uint16_t wr = w6_rd16(BSB_S1_REG, 0x020C);
        w6_write(BSB_S1_TX, wr, src, (int)chunk);
        w6_wr16(BSB_S1_REG, 0x020C, (uint16_t)(wr + chunk));
        w6_wr8(BSB_S1_REG, 0x0010, 0x20);
        uint32_t t = millis();
        uint8_t ir = 0;
        while (!((ir = w6_rd8(BSB_S1_REG, 0x0020)) & 0x18) && millis() - t < 50) {}
        w6_wr8(BSB_S1_REG, 0x0028, 0x18);
        if (!(ir & 0x10)) { web_abort_send(); return true; }
        if (g_webPhase == 1) g_webHdrOff = (uint16_t)(g_webHdrOff + chunk);
        else g_webBodyOff += chunk;
        used += chunk;
    }
    return g_webPhase == 0;
}

static void web_json_escape(char *dst, int max, const char *src)
{
    int i = 0;
    for (; src && *src && i < max - 1; src++) {
        char c = *src;
        if (c == '"' || c == '\\') continue;
        if ((uint8_t)c < 32) continue;
        dst[i++] = c;
    }
    dst[i] = 0;
}

// A sketch that takes the wall preview on its own UDP port defines PIC_PORT; the host
// reads "pic" from /status and sends VPIC there. Without it, VPIC stays on UDP_PORT.
#define WEB_STR2(x) #x
#define WEB_STR(x) WEB_STR2(x)
#ifdef PIC_PORT
#define WEB_PIC_FIELD ",\"pic\":" WEB_STR(PIC_PORT)
#else
#define WEB_PIC_FIELD ""
#endif

static void web_handle(char *req)
{
    char *sp = strchr(req, ' ');
    if (!sp) { web_queue("400 Bad Request", "text/plain", "", 0); return; }
    char *path = sp + 1;
    char *end = strchr(path, ' ');
    if (end) *end = 0;

    if (!strncmp(path, "/status", 7)) {
        char title[41];
        web_json_escape(title, sizeof(title), g_title);
        static char js[768];
        uint8_t phy = w6_rd8(BSB_COMMON, 0x3000);
        uint32_t fill = g_wframe - g_rframe;
        bool playing = g_lastPkt && (uint32_t)(millis() - g_lastPkt) < 250;
        int n = snprintf(js, sizeof(js),
            "{\"kind\":\"verpixeld-audio\",\"ip\":\"%u.%u.%u.%u\",\"mask\":\"%u.%u.%u.%u\",\"gw\":\"%u.%u.%u.%u\","
            "\"dhcp\":%d,\"link\":%d,\"spd\":%d,\"dpx\":%d,\"vol\":%u,\"mute\":%d,\"tone\":%d,\"live\":%d,\"codec\":%d,"
            "\"fill\":%lu,\"underrun\":%lu,\"overrun\":%lu,\"up\":%lu,"
            "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"host\":\"%s\",\"title\":\"%s\","
            "\"ver\":\"%s\",\"udp\":%d,\"web\":%d,\"spi\":%lu,\"spik\":%lu,\"peri\":%lu" WEB_PIC_FIELD "}",
            g_ip[0], g_ip[1], g_ip[2], g_ip[3],
            g_mask[0], g_mask[1], g_mask[2], g_mask[3],
            g_gw[0], g_gw[1], g_gw[2], g_gw[3],
            g_dhcpOk ? 1 : 0, (phy & 1) ? 1 : 0, (phy & 2) ? 10 : 100, (phy & 4) ? 0 : 1,
            (unsigned)g_volume, g_mute ? 1 : 0, g_tone ? 1 : 0, playing ? 1 : 0, g_codecOk ? 1 : g_dacMode ? 2 : 0,
            (unsigned long)fill, (unsigned long)g_underrun, (unsigned long)g_overrun,
            (unsigned long)(millis() / 1000),
            W6_MAC[0], W6_MAC[1], W6_MAC[2], W6_MAC[3], W6_MAC[4], W6_MAC[5],
            g_hostLabel, title, FW_VERSION, UDP_PORT, WEB_PORT,
            (unsigned long)(g_spiHz / 1000000u), (unsigned long)(g_spiHz / 1000u),
            (unsigned long)(clock_get_hz(clk_peri) / 1000000u));
        if (n < 0) n = 0;
        if (n >= (int)sizeof(js)) n = (int)sizeof(js) - 1;
        web_queue("200 OK", "application/json", js, (uint32_t)n);
    } else if (!strncmp(path, "/cmd", 4)) {
        char *q = strchr(path, '?');
        char *cv = q ? strstr(q, "c=") : nullptr;
        if (cv && !g_webCmdPending) {
            static char dec[96];
            web_url_decode(dec, cv + 2, sizeof(dec));
            strncpy((char *)g_webCmd, dec, sizeof(g_webCmd) - 1);
            ((char *)g_webCmd)[sizeof(g_webCmd) - 1] = 0;
            __sync_synchronize();
            g_webCmdPending = true;
        }
        static const char OK[] = "{\"ok\":1}";
        web_queue("200 OK", "application/json", OK, (uint32_t)(sizeof(OK) - 1));
    } else if (path[0] == '/' && (path[1] == 0 || !strncmp(path, "/index", 6))) {
        web_queue("200 OK", "text/html", WEB_PAGE, (uint32_t)(sizeof(WEB_PAGE) - 1));
    } else {
        web_queue("404 Not Found", "text/plain", "not found", 9);
    }
}

static volatile bool g_otaActive = false;

static void web_send_raw(const uint8_t *data, uint32_t len)
{
    uint32_t sent = 0;
    while (sent < len) {
        uint16_t fsr = 0;
        uint32_t t = millis();
        while ((fsr = w6_rd16(BSB_S1_REG, 0x0204)) == 0 && millis() - t < 200) {}
        if (fsr == 0) return;
        uint32_t chunk = len - sent;
        if (chunk > fsr) chunk = fsr;
        uint16_t wr = w6_rd16(BSB_S1_REG, 0x020C);
        w6_write(BSB_S1_TX, wr, data + sent, (int)chunk);
        w6_wr16(BSB_S1_REG, 0x020C, (uint16_t)(wr + chunk));
        w6_wr8(BSB_S1_REG, 0x0010, 0x20);
        t = millis();
        uint8_t ir = 0;
        while (!((ir = w6_rd8(BSB_S1_REG, 0x0020)) & 0x18) && millis() - t < 300) {}
        w6_wr8(BSB_S1_REG, 0x0028, 0x18);
        if (!(ir & 0x10)) return;
        sent += chunk;
    }
}

static void web_send(const char *status, const char *ctype, const char *body, uint32_t blen)
{
    char hdr[128];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
        status, ctype, (unsigned long)blen);
    web_send_raw((const uint8_t *)hdr, (uint32_t)hl);
    if (blen) web_send_raw((const uint8_t *)body, blen);
}

static void web_ota_receive(uint8_t *req, uint16_t firstN)
{
    req[firstN] = 0;
    char *cl = strstr((char *)req, "Content-Length:");
    if (!cl) cl = strstr((char *)req, "content-length:");
    long clen = cl ? atol(cl + 15) : -1;
    char *body = strstr((char *)req, "\r\n\r\n");
    if (clen <= 0 || clen > 1500000 || !body) {
        web_send("400 Bad Request", "text/plain", "bad OTA request", 15);
        return;
    }
    body += 4;
    uint16_t headerLen = (uint16_t)((uint8_t *)body - req);
    uint16_t bodyInBuf = (firstN > headerLen) ? (uint16_t)(firstN - headerLen) : 0;

    g_otaActive = true;
    __sync_synchronize();

    if (!Update.begin((size_t)clen, U_FLASH)) {
        g_otaActive = false;
        web_send("507 Insufficient Storage", "text/plain", "OTA begin failed - built with an FS partition?", 46);
        return;
    }

    uint32_t written = 0;
    bool werr = false;
    if (bodyInBuf) {
        werr = (Update.write((uint8_t *)body, bodyInBuf) != bodyInBuf);
        written += bodyInBuf;
    }

    static uint8_t chunk[2048];
    uint32_t lastData = millis();
    while (!werr && written < (uint32_t)clen) {
        uint16_t rsr = w6_rd16(BSB_S1_REG, 0x0224);
        if (rsr == 0) {
            if (millis() - lastData > 10000) break;
            continue;
        }
        uint32_t remain = (uint32_t)clen - written;
        uint16_t want = (uint16_t)(rsr < remain ? rsr : remain);
        if (want > sizeof(chunk)) want = sizeof(chunk);
        uint16_t rd = w6_rd16(BSB_S1_REG, 0x0228);
        w6_read(BSB_S1_RX, rd, chunk, want);
        w6_wr16(BSB_S1_REG, 0x0228, (uint16_t)(rd + want));
        w6_wr8(BSB_S1_REG, 0x0010, 0x40);
        if (Update.write(chunk, want) != want) { werr = true; break; }
        written += want;
        lastData = millis();
    }

    if (!werr && written == (uint32_t)clen && Update.end(true)) {
        web_send("200 OK", "text/plain", "OTA OK - rebooting", 18);
        delay(400);
        rp2040.reboot();
    } else {
        Update.end(false);
        g_otaActive = false;
        web_send("500 Internal Server Error", "text/plain", "OTA failed (size/write/verify)", 30);
    }
}

static void web_poll()
{
    uint8_t sr = w6_rd8(BSB_S1_REG, 0x0030);
    if (sr != 0x17 && g_webPhase) web_abort_send();
    switch (sr) {
    case 0x00:
        w6_wr8(BSB_S1_REG, 0x0000, 0x01);
        w6_wr16(BSB_S1_REG, 0x0114, WEB_PORT);
        w6_wr8(BSB_S1_REG, 0x0010, 0x01);
        break;
    case 0x13:
        w6_wr8(BSB_S1_REG, 0x0010, 0x02);
        break;
    case 0x17: {
        if (g_webPhase) {
            if (web_send_tick()) w6_wr8(BSB_S1_REG, 0x0010, 0x08);
            break;
        }
        uint16_t rsr = w6_rd16(BSB_S1_REG, 0x0224);
        if (rsr == 0) break;
        static uint8_t req[768];
        uint16_t n = rsr > (uint16_t)(sizeof(req) - 1) ? (uint16_t)(sizeof(req) - 1) : rsr;
        uint16_t rd = w6_rd16(BSB_S1_REG, 0x0228);
        w6_read(BSB_S1_RX, rd, req, n);
        req[n] = 0;
        if (!strncmp((char *)req, "POST /update", 12)) {
            w6_wr16(BSB_S1_REG, 0x0228, (uint16_t)(rd + n));
            w6_wr8(BSB_S1_REG, 0x0010, 0x40);
            web_ota_receive(req, n);
            w6_wr8(BSB_S1_REG, 0x0010, 0x08);
        } else {
            uint16_t extra = (uint16_t)(rsr - n);
            w6_wr16(BSB_S1_REG, 0x0228, (uint16_t)(rd + n + extra));
            w6_wr8(BSB_S1_REG, 0x0010, 0x40);
            web_handle((char *)req);
            if (web_send_tick()) w6_wr8(BSB_S1_REG, 0x0010, 0x08);
        }
        break;
    }
    case 0x1C:
        w6_wr8(BSB_S1_REG, 0x0010, 0x08);
        break;
    default:
        break;
    }
}

static void handleWebCmd()
{
    if (!g_webCmdPending) return;
    __sync_synchronize();
    char cmd[sizeof(g_webCmd)];
    strncpy(cmd, (const char *)g_webCmd, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = 0;
    g_webCmdPending = false;
    Serial.printf("[web] %s\n", cmd);
    handleLine(cmd);
}
