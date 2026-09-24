#include "smith_web.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

extern void smithSetStage(uint8_t stage);
extern void smithSendCustom(uint8_t node_id, const char* msg);
extern uint8_t smithGetStage();

static WebServer g_server(80);

static const char PAGE[] PROGMEM = R"END_PAGE(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Matrix Admin</title>
<style>
body{background:#000;color:#00ff41;font-family:monospace;padding:20px;max-width:500px;margin:0 auto}
h1{border-bottom:1px solid #005500;padding-bottom:10px}
.box{border:1px solid #005500;padding:15px;margin-bottom:20px;background:#001100}
button{background:#003300;color:#00ff41;border:1px solid #00ff41;padding:10px;cursor:pointer;margin:5px 0;width:100%;font-weight:bold}
button:hover{background:#005500}
.active{background:#ff2020;color:#fff;border-color:#ff2020}
input,select{background:#000;color:#00ff41;border:1px solid #00ff41;padding:8px;width:100%;margin-bottom:10px;box-sizing:border-box}
label{display:block;margin-bottom:5px;font-size:0.9rem}
</style>
</head>
<body>
<h1>Matrix Admin Panel</h1>

<div class="box">
<h2>Virus Stage Control</h2>
<button onclick="setS(0)" id="b0">0: Stealth (Off)</button>
<button onclick="setS(1)" id="b1">1: Impersonate / Ping</button>
<button onclick="setS(2)" id="b2">2: Corrupt Mode</button>
<button onclick="setS(3)" id="b3">3: Flood Network</button>
</div>

<div class="box">
<h2>Manual Injection</h2>
<label>Spoof Node ID (1-50, or 99 for Smith)</label>
<input type="number" id="node" value="99" min="1" max="99">
<label>Message</label>
<input type="text" id="msg" maxlength="180" placeholder="Type here..." autocomplete="off">
<button onclick="sendM()">INJECT PAYLOAD</button>
</div>

<script>
async function setS(v) {
  await fetch('/stage',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'stage='+v});
  updateBtn(v);
}
function updateBtn(s) {
  for(let i=0;i<=3;i++) document.getElementById('b'+i).className=(i==s)?'active':'';
}
async function sendM() {
  const n=document.getElementById('node').value;
  const m=document.getElementById('msg').value.trim();
  if(!m) return;
  await fetch('/send',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'node='+n+'&msg='+encodeURIComponent(m)});
  document.getElementById('msg').value='';
}
updateBtn(CURRENT_STAGE_PLACEHOLDER);
</script>
</body>
</html>
)END_PAGE";

static void handleRoot()
{
    String html = PAGE;
    html.replace("CURRENT_STAGE_PLACEHOLDER", String(smithGetStage()));
    g_server.send(200, "text/html", html);
}

static void handleStage()
{
    if (g_server.hasArg("stage"))
    {
        uint8_t stage = (uint8_t)g_server.arg("stage").toInt();
        if (stage <= 3) {
            smithSetStage(stage);
        }
    }
    g_server.send(200, "text/plain", "ok");
}

static void handleSend()
{
    if (g_server.hasArg("node") && g_server.hasArg("msg"))
    {
        uint8_t node = (uint8_t)g_server.arg("node").toInt();
        String msg = g_server.arg("msg");
        msg.trim();
        if (msg.length() > 0 && msg.length() <= 180)
        {
            smithSendCustom(node, msg.c_str());
        }
    }
    g_server.send(200, "text/plain", "ok");
}

void smithWebBegin()
{
    WiFi.softAP("Matrix-Admin", "admin123");
    
    g_server.on("/", HTTP_GET, handleRoot);
    g_server.on("/stage", HTTP_POST, handleStage);
    g_server.on("/send", HTTP_POST, handleSend);
    g_server.begin();
    
    Serial.println("[smith] Hacker Web UI started at http://192.168.4.1");
}

void smithWebLoop()
{
    g_server.handleClient();
}
