// =============================================================
//  web.cpp — Matrix-themed chat web UI + captive portal.
//
//  Architecture:
//    • Arduino WebServer (blocking but brief) on port 80
//    • DNSServer redirects all DNS queries to 192.168.4.1
//    • GET /         → serve the embedded HTML page
//    • GET /api      → JSON snapshot (smith state + messages)
//    • POST /send    → receive a new chat message
//    • GET /generate_204, /hotspot-detect.html, /ncsi.txt
//                   → OS captive-portal check redirects
//    • Rate-limit: 1 message / second per client IP
// =============================================================
#include "web.h"
#include "palette.h"

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFi.h>

// ----- Module state ----------------------------------------
static WebServer  g_server(80);
static DNSServer  g_dns;

static uint8_t  g_node_id   = 1;
static char     g_node_name[24] = "Neo";

static uint8_t  g_smith_state = 0;    // 0=CLEAR 1=NEAR 2=CLOSE
static float    g_smith_rssi  = -100.0f;

// ----- Message history (ring buffer) -----------------------
static constexpr uint8_t HIST_SIZE = 16;
struct WebMsg {
    uint8_t node_id;
    char    name[24];
    uint8_t r, g, b;
    char    text[181];
    bool    is_smith;
};
static WebMsg  g_hist[HIST_SIZE] = {};
static uint8_t g_hist_head  = 0;
static uint8_t g_hist_count = 0;

// ----- Rate limiting ---------------------------------------
static uint32_t g_last_send_ms  = 0;
static constexpr uint32_t RATE_LIMIT_MS = 1000;

// ----- Helpers ---------------------------------------------
static String htmlEscape(const char* s)
{
    String out;
    for (; *s; ++s) {
        switch (*s) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            default:   out += *s;
        }
    }
    return out;
}

static String jsonEscape(const char* s)
{
    String out;
    for (; *s; ++s) {
        if (*s == '"' || *s == '\\') out += '\\';
        if (*s == '\n') { out += "\\n"; continue; }
        if (*s == '\r') continue;
        out += *s;
    }
    return out;
}

static String colorHex(uint8_t r, uint8_t g, uint8_t b)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return String(buf);
}

// ----- Embedded HTML page ----------------------------------
static const char PAGE[] PROGMEM = R"END_PAGE(
<!doctype html><html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#000000">
<title id="pt">Matrix Chat</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;color:#00ff41;font-family:'Courier New',monospace;overflow-x:hidden}
#rain{position:fixed;top:0;left:0;z-index:0;opacity:.3;pointer-events:none}
main{position:relative;z-index:1;max-width:600px;margin:0 auto;padding:12px;height:100dvh;display:flex;flex-direction:column;gap:8px}
h1{font-size:1rem;letter-spacing:.15em;color:#00ff41;text-shadow:0 0 8px #00ff41;padding:8px 0;border-bottom:1px solid #003300;flex-shrink:0}
#bar{display:flex;align-items:center;justify-content:space-between;padding:8px 12px;border-radius:6px;background:rgba(0,30,0,.8);border:1px solid #003300;flex-shrink:0;font-size:.82rem;gap:8px}
#badge{font-weight:700;letter-spacing:.08em;white-space:nowrap}
.cl{color:#00ff41}.nr{color:#ffcc00;animation:pls 1s ease-in-out infinite}.cs{color:#ff2020;animation:pls .3s ease-in-out infinite}
#rssi-val{font-size:.75rem;color:#669966;margin-left:4px}
#spark{width:80px;height:28px;flex-shrink:0}
#msgs{flex:1;overflow-y:auto;display:flex;flex-direction:column;gap:5px;padding:4px 0}
.msg{background:rgba(0,15,0,.8);border:1px solid #002200;border-radius:6px;padding:7px 10px}
.msg.sm{border-color:#440000;background:rgba(25,0,0,.85)}
.who{font-size:.72rem;font-weight:700;letter-spacing:.08em;margin-bottom:2px}
.body{font-size:.88rem;word-break:break-word;line-height:1.4}
.sm .body{color:#ff4040;animation:glitch .5s step-end infinite}
form{display:flex;gap:8px;flex-shrink:0}
input{flex:1;background:rgba(0,15,0,.8);border:1px solid #005500;color:#00ff41;padding:9px 11px;border-radius:6px;font-family:inherit;font-size:.9rem;outline:none;min-width:0}
input:focus{border-color:#00ff41;box-shadow:0 0 5px rgba(0,255,65,.25)}
button{background:#002200;border:1px solid #005500;color:#00ff41;padding:9px 14px;border-radius:6px;font-family:inherit;cursor:pointer;letter-spacing:.06em;flex-shrink:0;transition:background .15s}
button:hover{background:#004400}
@keyframes pls{0%,100%{opacity:1}50%{opacity:.35}}
@keyframes glitch{0%{text-shadow:2px 0 #f00,-2px 0 #0f0}25%{text-shadow:-2px 0 #f00,2px 0 #0f0}50%{text-shadow:2px 0 #0f0,-2px 0 #f00}75%,100%{text-shadow:none}}
::-webkit-scrollbar{width:3px}::-webkit-scrollbar-thumb{background:#003300;border-radius:2px}
</style></head><body>
<canvas id="rain"></canvas>
<main>
<h1 id="pt">Matrix Chat</h1>
<div id="bar">
  <div><span id="badge" class="cl">■ CLEAR</span><span id="rssi-val"></span></div>
  <svg id="spark" viewBox="0 0 80 28" preserveAspectRatio="none">
    <polyline id="spl" fill="none" stroke="#00ff41" stroke-width="1.5" points=""/>
  </svg>
</div>
<div id="msgs"><div class="msg"><div class="body" style="color:#336633">Connecting to the Matrix...</div></div></div>
<form id="frm">
  <input id="txt" maxlength="95" placeholder="Enter the Matrix..." autocomplete="off">
  <button type="submit">SEND</button>
</form>
</main>
<script>
// Digital rain
(function(){
const cv=document.getElementById('rain'),cx=cv.getContext('2d');
const CH='アイウエオカキクケコサシスセソタチツテト0123456789ABCDEF01';
let dr=[];
function rsz(){cv.width=window.innerWidth;cv.height=window.innerHeight;dr=Array.from({length:Math.floor(cv.width/14)},()=>Math.random()*-60);}
rsz();window.addEventListener('resize',rsz);
setInterval(()=>{
  cx.fillStyle='rgba(0,0,0,0.05)';cx.fillRect(0,0,cv.width,cv.height);
  cx.fillStyle='#00ff41';cx.font='14px monospace';
  dr.forEach((y,i)=>{cx.fillText(CH[Math.floor(Math.random()*CH.length)],i*14,y*14);if(y*14>cv.height&&Math.random()>.975)dr[i]=0;dr[i]++;});
},50);
})();

// Chat
const badge=document.getElementById('badge'),rssiVal=document.getElementById('rssi-val'),msgs=document.getElementById('msgs'),spl=document.getElementById('spl');
const SC=['cl','nr','cs'],SL=['■ CLEAR','▲ NEAR','● CLOSE'];
let rh=[],lastN=0,title=document.getElementById('pt');
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function spark(){
  if(rh.length<2)return;
  const mn=-90,mx=-30,w=80,h=28;
  spl.setAttribute('points',rh.map((v,i)=>{
    const x=(i/(rh.length-1))*w;
    const y=h-((Math.max(mn,Math.min(mx,v))-mn)/(mx-mn))*h;
    return x+','+y;
  }).join(' '));
}
async function refresh(){
  try{
    const d=await(await fetch('/api')).json();
    title.textContent=d.name+' | Matrix Chat';
    const si=d.smith;
    badge.className='badge '+SC[si];badge.textContent=SL[si]||'CLEAR';
    rssiVal.textContent=si>0?' ('+d.rssi.toFixed(0)+'dBm)':'';
    rh.push(d.rssi);if(rh.length>20)rh.shift();spark();
    if(d.messages.length!==lastN){
      lastN=d.messages.length;msgs.innerHTML='';
      d.messages.forEach(m=>{
        const div=document.createElement('div');
        div.className='msg'+(m.smith?' sm':'');
        div.innerHTML='<div class="who" style="color:'+m.col+'">'+esc(m.name)+'</div><div class="body">'+esc(m.text)+'</div>';
        msgs.appendChild(div);
      });
      msgs.scrollTop=msgs.scrollHeight;
    }
  }catch(e){}
}
document.getElementById('frm').onsubmit=async e=>{
  e.preventDefault();
  const inp=document.getElementById('txt'),v=inp.value.trim();
  if(!v)return;
  inp.value='';
  await fetch('/send',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'message='+encodeURIComponent(v)});
  refresh();
};
refresh();setInterval(refresh,1000);
</script></body></html>
)END_PAGE";

// ----- HTTP handlers ---------------------------------------
static void handleRoot()
{
    g_server.send_P(200, "text/html", PAGE);
}

static void handleApi()
{
    // Build JSON snapshot
    String json = "{\"name\":\"";
    json += jsonEscape(g_node_name);
    json += "\",\"smith\":";
    json += String(g_smith_state);
    json += ",\"rssi\":";
    // Avoid sprintf/float formatting issues — use integer + decimal
    int r_int = (int)g_smith_rssi;
    json += String(r_int);
    json += ".0,\"messages\":[";

    for (uint8_t i = 0; i < g_hist_count; ++i)
    {
        const uint8_t idx = (g_hist_head + HIST_SIZE - g_hist_count + i) % HIST_SIZE;
        const WebMsg& m   = g_hist[idx];
        if (i > 0) json += ',';
        json += "{\"id\":";
        json += String(m.node_id);
        json += ",\"name\":\"";
        json += jsonEscape(m.name);
        json += "\",\"col\":\"";
        json += colorHex(m.r, m.g, m.b);
        json += "\",\"text\":\"";
        json += jsonEscape(m.text);
        json += "\",\"smith\":";
        json += m.is_smith ? "true" : "false";
        json += "}";
    }
    json += "]}";

    g_server.send(200, "application/json", json);
}

static void handleSend()
{
    if (!g_server.hasArg("message"))
    {
        g_server.send(400, "text/plain", "missing message");
        return;
    }

    // Rate limit: 1 message per second per node (single-client workshop).
    uint32_t now = millis();
    if ((now - g_last_send_ms) < RATE_LIMIT_MS)
    {
        g_server.send(429, "text/plain", "slow down");
        return;
    }

    String msg = g_server.arg("message");
    msg.trim();
    if (msg.length() == 0 || msg.length() > 180)
    {
        g_server.send(400, "text/plain", "message 1-180 chars");
        return;
    }

    g_last_send_ms = now;
    sendChatMessage(msg.c_str());
    g_server.send(200, "text/plain", "ok");
}

// Redirect captive-portal OS probes back to the chat page.
static void handleCaptive()
{
    g_server.sendHeader("Location", "http://192.168.4.1/", true);
    g_server.send(302, "text/plain", "");
}

// ----- Public API ------------------------------------------
void webBegin(uint8_t node_id, const char* node_name)
{
    g_node_id = node_id;
    strncpy(g_node_name, node_name, sizeof(g_node_name) - 1);
    g_node_name[sizeof(g_node_name) - 1] = '\0';

    // SoftAP: "NEO-<id>" — unique per node, easy to identify.
    char ap_name[32];
    snprintf(ap_name, sizeof(ap_name), "NEO-%u", node_id);
    WiFi.softAP(ap_name, "matrix123");  // channel inherited from radioInit()

    // DNSServer: redirect all DNS queries to the AP IP.
    g_dns.start(53, "*", WiFi.softAPIP());

    g_server.on("/",                    HTTP_GET,  handleRoot);
    g_server.on("/api",                 HTTP_GET,  handleApi);
    g_server.on("/send",                HTTP_POST, handleSend);
    // OS captive-portal probe URLs (Android, iOS, Windows):
    g_server.on("/generate_204",        HTTP_GET,  handleCaptive);
    g_server.on("/hotspot-detect.html", HTTP_GET,  handleCaptive);
    g_server.on("/ncsi.txt",            HTTP_GET,  handleCaptive);
    g_server.onNotFound(handleCaptive);

    g_server.begin();
}

void webLoop()
{
    g_dns.processNextRequest();
    g_server.handleClient();
}

void webAddMessage(uint8_t node_id, const char* name,
                   uint8_t r, uint8_t g, uint8_t b,
                   const char* text, bool is_smith)
{
    WebMsg& m   = g_hist[g_hist_head];
    m.node_id   = node_id;
    strncpy(m.name, name, sizeof(m.name) - 1);
    m.name[sizeof(m.name) - 1] = '\0';
    m.r = r; m.g = g; m.b = b;
    strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = '\0';
    m.is_smith  = is_smith;

    g_hist_head = (g_hist_head + 1) % HIST_SIZE;
    if (g_hist_count < HIST_SIZE) ++g_hist_count;
}

void webSetSmithStatus(uint8_t state, float rssi_f)
{
    g_smith_state = state;
    g_smith_rssi  = rssi_f;
}
