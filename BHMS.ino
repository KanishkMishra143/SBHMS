// Bridge Health Monitor Dashboard - updated Wi-Fi and ADXL345 recovery
// Wiring preserved: I2C SDA=21, SCL=22; RGB=14/26/27; buzzer=25.
// Load/HX711 remains simulated/not read, as in the original sketch.

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Wi-Fi credentials: tries TEST first, then Hathway_2.4G.
const char* WIFI1 = "Test";
const char* PASS1 = "9354706457";
const char* WIFI2 = "Hathway_2.4G";
const char* PASS2 = "9354706457@9717259475";
const char* HOST = "dashboard";

#define I2C_SDA 21
#define I2C_SCL 22
#define LED_RED 14
#define LED_GREEN 26
#define LED_BLUE 27
#define BUZZER_PIN 25
#define HX711_DOUT 33 // reserved; not read
#define HX711_SCK 32  // reserved; not read

#define ADXL_ADDR 0x53
#define REG_DEVID 0x00
#define REG_BW_RATE 0x2C
#define REG_POWER_CTL 0x2D
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0 0x32
#define G_PER_LSB 0.0039f
#define SAMPLE_US 5000UL

LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);
enum Screen { LOAD_SCREEN, ADXL_SCREEN };
Screen screenNow = LOAD_SCREEN;

bool adxlReady=false, streamEnabled=true, autoMode=true, buzzerOn=false;
bool manualR=false, manualG=false, manualB=false, wifiReady=false;
bool serverStarted = false;
bool mdnsStarted = false;
float loadKg=0, ax=0, ay=0, az=0, mag=0;
// Initial prototype vibration thresholds; calibrate on the mounted model.
float lpX=0, lpY=0, lpZ=0, hpX=0, hpY=0, hpZ=0, vibRms=0, vibPeak=0;
bool filterInitialized=false;
const float HP_ALPHA=0.0305f; // ~1 Hz high-pass at 200 Hz
// Prototype thresholds: tune against repeatable tests on the mounted model.
// Warning: filtered RMS >= 0.040 g continuously for 3 seconds.
// Critical: 4 or more distinct filtered peaks >= 0.300 g within a rolling 3-second window.
const float VIB_WARN_RMS=0.040f, VIB_WARN_CLEAR_RMS=0.030f;
const float VIB_CRIT_CLEAR_RMS=0.025f;
const float VIB_IMPACT_PEAK=0.300f;
const unsigned long VIB_SUSTAINED_MS=3000UL;
const unsigned long VIB_PEAK_WINDOW_MS=3000UL;
const unsigned long VIB_MIN_PEAK_SPACING_MS=150UL;
const uint8_t VIB_PEAKS_FOR_CRITICAL=4; // more than 3 peaks in 3 seconds
const uint16_t VIB_WINDOW=200;
float vibSquares[VIB_WINDOW]={0};
uint16_t vibWindowHead=0, vibWindowCount=0;
float vibSquareSum=0;
unsigned long vibWarnSince=0, vibClearSince=0, peakWindowStarted=0;
unsigned long vibPeakTimes[10]={0};
uint8_t vibPeakHead=0, vibPeakStored=0, vibPeaksInWindow=0;
float previousFilteredV=0;
unsigned long lastVibPeakAt=0;
uint8_t vibrationSeverity=0, overallSeverity=0;
String vibrationReason="Warming up", overallReason="Bridge status normal";
unsigned long lastAdxlRetry = 0;
uint8_t consecutiveAdxlErrors = 0;
const float WARN_KG=4.0f, DANGER_KG=5.0f;
const unsigned long BEEP_EVERY=1000, BEEP_MS=150; // short rhythmic beep once per second for warning/critical
uint32_t nextSample=0, lastLCD=0;
unsigned long beepStarted=0, lastWifiRetry=0, lastSampleMs=0;
uint32_t sampleCount=0;
uint32_t alertSeq = 0;
uint8_t alertLevel = 0; // severity of the most recent toast event
String alertText = "";
// Track each source independently so a new vibration event is announced even
// while a load alert is already keeping the overall bridge state CRITICAL.
uint8_t lastObservedLoadSeverity = 0;
uint8_t lastObservedVibrationSeverity = 0;
struct PlotSample { uint32_t seq; float x, y, z, magnitude; };
const uint16_t SAMPLE_BUFFER_SIZE = 256;
PlotSample sampleBuffer[SAMPLE_BUFFER_SIZE];
uint16_t sampleHead = 0;
uint16_t sampleCountBuffered = 0;
uint32_t lastSerialPlotMs = 0;
const uint32_t SERIAL_PLOT_INTERVAL_MS = 20;
char cmdBuffer[64]; uint8_t cmdLen=0;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bridge Monitor</title><style>
:root{color-scheme:dark;--bg:#0b1020;--p:#151f34;--line:#2b3b58;--txt:#edf2ff;--mut:#9eacc8;--blue:#70a5ff;--green:#35d39a;--yellow:#ffca62;--red:#ff6b78}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px system-ui}header{padding:14px;background:#10192b;border-bottom:1px solid var(--line);position:sticky;top:0}header,main{max-width:1150px;margin:auto}header{max-width:none}.top{max-width:1150px;margin:auto;display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap}.brand{font-weight:800;font-size:19px}.muted,.note{color:var(--mut);font-size:12px}nav,.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}main{padding:20px 14px}.page{display:none}.page.active{display:block}.grid{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:12px}.card{background:var(--p);border:1px solid var(--line);border-radius:14px;padding:15px;min-width:0}.s12{grid-column:span 12}.s8{grid-column:span 8}.s6{grid-column:span 6}.s4{grid-column:span 4}h2{margin:0 0 5px}h3{font-size:16px;margin:0 0 12px}button{background:#202e49;border:1px solid var(--line);border-radius:9px;color:var(--txt);padding:10px 12px;font:inherit;cursor:pointer}button.active,.primary{background:var(--blue);color:#081225}.pagebtn{margin:0}.metric{background:#0c1425;border:1px solid var(--line);padding:11px;border-radius:10px;min-width:0}.metrics{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}.label{color:var(--mut);font-size:12px}.value{font-size:23px;font-weight:800;overflow-wrap:anywhere}.unit{font-size:12px;color:var(--mut)}#toastHost{position:fixed;right:20px;bottom:20px;top:auto;left:auto;z-index:9999;display:flex;flex-direction:column;gap:10px;max-width:min(390px,calc(100vw - 32px));pointer-events:none}.toast{padding:14px 16px;border-radius:12px;border:1px solid var(--line);background:#151f34;box-shadow:0 12px 32px #0008;animation:toastIn .18s ease-out;border-left:5px solid var(--blue)}.toast.warning{border-left-color:var(--yellow)}.toast.critical{border-left-color:var(--red)}.toastTitle{font-weight:800;margin-bottom:4px}.toastBody{font-size:13px;color:var(--mut);line-height:1.4}@keyframes toastIn{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:translateY(0)}}canvas{width:100%;height:280px;background:#0b1323;border-radius:10px;display:block}.legend{display:flex;gap:12px;flex-wrap:wrap;color:var(--mut);font-size:12px;margin-top:8px}.legend i{display:inline-block;width:9px;height:9px;border-radius:3px;background:var(--c);margin-right:4px}.status{display:inline-block;border:1px solid var(--line);border-radius:20px;padding:6px 9px;font-size:12px}.keydisplay{font-size:25px;font-weight:800;background:#0b1323;border:1px solid var(--line);padding:12px;border-radius:9px;margin-bottom:8px}.keys{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;max-width:340px}.keys button{min-height:44px;font-size:18px}.wide{grid-column:1/-1}.toggle{display:flex;justify-content:space-between;align-items:center;gap:12px}.note{line-height:1.5;margin-top:8px}@media(max-width:760px){.s8,.s6,.s4{grid-column:span 12}.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}canvas{height:240px}}
</style></head><body><div id="toastHost" aria-live="polite" aria-atomic="false"></div><header><div class="top"><div><div class="brand">Bridge Health Monitor</div><div class="muted" id="net">Connecting…</div></div><nav><button id="homeTab" class="active" onclick="page('home')">Home</button><button id="manualTab" onclick="page('manual')">Manual Override</button></nav></div></header><main>
<section id="home" class="page active"><div class="row" style="justify-content:space-between;margin-bottom:12px"><div><h2>Home</h2><span class="muted">Live vibration graph and LCD values mirrored here</span></div><span class="status" id="bridgeState">BRIDGE NORMAL</span><span class="status" id="health">Sensor starting…</span></div>
<div class="grid"><div class="card s12"><h3>Continuous vibration graph · latest samples</h3><canvas id="chart"></canvas><div class="legend"><span><i style="--c:#70a5ff"></i>X</span><span><i style="--c:#35d39a"></i>Y</span><span><i style="--c:#ffca62"></i>Z</span><span><i style="--c:#e5a2ff"></i>Magnitude</span></div></div>
<div class="card s8"><h3>Accelerometer</h3><div class="metrics"><div class="metric"><div class="label">X (g)</div><div class="value" id="ax">—</div></div><div class="metric"><div class="label">Y (g)</div><div class="value" id="ay">—</div></div><div class="metric"><div class="label">Z (g)</div><div class="value" id="az">—</div></div><div class="metric"><div class="label">Magnitude (g)</div><div class="value" id="mag">—</div></div></div><div class="metrics" style="margin-top:8px;grid-template-columns:repeat(3,minmax(0,1fr))"><div class="metric"><div class="label">Filtered RMS (g)</div><div class="value" id="vibRms">—</div></div><div class="metric"><div class="label">Window peak (g)</div><div class="value" id="vibPeak">—</div></div><div class="metric"><div class="label">Vibration state</div><div class="value" id="vibState" style="font-size:17px">STARTING</div></div></div>
<div class="card s4"><h3>Load status</h3><div class="metric"><div class="label">Simulated load</div><div class="value" id="load">0.00 <span class="unit">kg</span></div></div><p><span class="status" id="loadState">NORMAL</span> <span class="status" id="mode">AUTO</span></p><div class="note">Load cell/HX711 is not read in this demo.</div></div>
<div class="card s6"><h3>LCD · LOAD screen</h3><div class="metric"><div class="label">Line 1</div><b id="ll1">Load: 0.00 kg</b></div><div class="metric" style="margin-top:7px"><div class="label">Line 2</div><b id="ll2">NORMAL</b></div></div>
<div class="card s6"><h3>LCD · ADXL screen</h3><div class="metric"><div class="label">Line 1</div><b id="al1">Mag: 0.00g</b></div><div class="metric" style="margin-top:7px"><div class="label">Line 2</div><b id="al2">X:0.0 Y:0.0</b></div><div class="note">Both LCD screens are mirrored regardless of which one is selected physically.</div></div></div></section>
<section id="manual" class="page"><h2>Manual Override</h2><p class="muted">Control hardware and simulated load.</p><div class="grid">
<div class="card s12 toggle"><div><h3 style="margin:0 0 4px">Automatic mode</h3><div class="note">ON: load and vibration control LED/buzzer. OFF: manual controls enabled.</div></div><button id="autoBtn" onclick="autoToggle()">AUTO: ON</button></div>
<div class="card s6"><h3>LCD screen selection</h3><div class="row"><button onclick="cmd('SCREEN LOAD')">LOAD screen</button><button onclick="cmd('SCREEN ADXL')">ADXL screen</button></div></div>
<div class="card s6"><h3>RGB LED</h3><div class="row"><button onclick="cmd('R ON')">Red</button><button onclick="cmd('G ON')">Green</button><button onclick="cmd('B ON')">Blue</button><button onclick="cmd('LED OFF')">All OFF</button></div><div class="note">Requires AUTO OFF.</div></div>
<div class="card s6"><h3>Buzzer</h3><div class="row"><button onclick="cmd('BUZZER ON')">Buzzer ON</button><button onclick="cmd('BUZZER OFF')">Buzzer OFF</button></div><div class="note">Requires AUTO OFF.</div></div>
<div class="card s6"><h3>Simulated load keypad (kg)</h3><div class="keydisplay" id="keyval">0</div><div class="keys"><button onclick="key('7')">7</button><button onclick="key('8')">8</button><button onclick="key('9')">9</button><button onclick="key('4')">4</button><button onclick="key('5')">5</button><button onclick="key('6')">6</button><button onclick="key('1')">1</button><button onclick="key('2')">2</button><button onclick="key('3')">3</button><button onclick="key('.')">.</button><button onclick="key('0')">0</button><button onclick="key('back')">⌫</button><button class="wide primary" onclick="sendLoad()">ENTER / SEND</button><button class="wide" onclick="clearKey()">CLEAR</button></div><div class="note">Range: 0–999.99 kg. Updates LCD and Home immediately. Simulated only.</div></div>
<div class="card s6"><h3>ADXL serial stream</h3><div class="toggle"><span>USB Serial Plotter output (labelled X/Y/Z/MAG)</span><button id="streamBtn" onclick="streamToggle()">STREAM: ON</button></div><div class="note">Serial Plotter starts enabled by default. Each series is labelled X, Y, Z, or MAG. The web graph runs independently.</div><hr style="border-color:var(--line)"><h3>Current state</h3><div class="row"><span class="status">Screen: <b id="scr">LOAD</b></span><span class="status">ADXL: <b id="sensor">—</b></span></div><div class="row" style="margin-top:8px"><span class="status">RGB: <b id="rgb">GREEN</b></span><span class="status">Buzzer: <b id="buzz">OFF</b></span></div><button style="margin-top:10px" onclick="refresh()">Refresh status</button><div class="note" id="msg">Ready.</div></div>
</div></section><p class="note">Prototype dashboard · Wi-Fi local network · load input is simulated.</p></main>
<script>
const $=x=>document.getElementById(x);let s=null,kv='0',pts=[],stream=false;
function page(p){['home','manual'].forEach(x=>$(x).classList.toggle('active',x===p));$('homeTab').classList.toggle('active',p==='home');$('manualTab').classList.toggle('active',p==='manual')}
let pollingPaused=false, dataBusy=false, statusBusy=false, lastSeq=0;
let lastAlertSeq=null;
function showToast(level,title,message){
  const host=$('toastHost');
  const el=document.createElement('div');
  el.className='toast '+(level===2?'critical':'warning');
  const titleEl=document.createElement('div'); titleEl.className='toastTitle'; titleEl.textContent=title;
  const bodyEl=document.createElement('div'); bodyEl.className='toastBody'; bodyEl.textContent=message;
  el.appendChild(titleEl); el.appendChild(bodyEl); host.appendChild(el);
  while(host.children.length>4)host.removeChild(host.firstChild);
  setTimeout(()=>{el.style.transition='opacity .25s,transform .25s';el.style.opacity='0';el.style.transform='translateY(-8px)';setTimeout(()=>el.remove(),280)},6500);
}
function checkAlert(x){
  const seq=Number(x.alertSeq||0);
  if(lastAlertSeq===null){lastAlertSeq=seq;return;}
  if(seq===lastAlertSeq)return;
  lastAlertSeq=seq;
  if(!x.auto)return;
  const level=Number(x.alertLevel);
  showToast(level,level===2?'CRITICAL BRIDGE ALERT':'BRIDGE HEALTH WARNING',x.alertText||'Bridge condition changed.');
}
async function cmd(c){
  pollingPaused=true;
  try{
    const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'text/plain'},body:c,cache:'no-store'});
    const t=await r.text();
    if(!r.ok)throw Error(t);
    $('msg').textContent=t;
    await refresh();
  }catch(e){$('msg').textContent='Command failed: '+e.message}
  finally{pollingPaused=false}
}
function key(k){if(k==='back')kv=kv.length>1?kv.slice(0,-1):'0';else if(k==='.') {if(!kv.includes('.'))kv+='.'}else kv=kv==='0'?k:kv+k;if(kv.length>8)kv=kv.slice(0,8);$('keyval').textContent=kv}
function clearKey(){kv='0';$('keyval').textContent=kv}
async function sendLoad(){let n=Number(kv);if(!Number.isFinite(n)||n<0||n>999.99){$('msg').textContent='Use 0–999.99 kg';return}await cmd('LOAD '+n.toFixed(2));clearKey()}
function autoToggle(){cmd(s&&s.auto?'AUTO OFF':'AUTO ON')}function streamToggle(){cmd(stream?'STREAM OFF':'STREAM ON')}
async function refresh(){
  if(statusBusy)return;
  statusBusy=true;
  try{let r=await fetch('/api/status',{cache:'no-store'});s=await r.json();render(s);checkAlert(s)}
  catch(e){$('net').textContent='Dashboard connection issue'}
  finally{statusBusy=false}
}
function render(x){
  $('net').textContent=x.wifi?'Connected · '+x.ip:'Wi-Fi offline';
  $('health').textContent=x.adxl?'ADXL345 online':'ADXL345 not detected';$('bridgeState').textContent='BRIDGE '+(x.overallState||'NORMAL');
  ['ax','ay','az'].forEach(k=>$(k).textContent=Number(x[k]).toFixed(3));
  $('mag').textContent=Number(x.mag).toFixed(3);
  $('vibRms').textContent=Number(x.vibRms||0).toFixed(3);
  $('vibPeak').textContent=Number(x.vibPeak||0).toFixed(3);
  $('vibState').textContent=x.vibState||'STARTING';
  $('load').innerHTML=Number(x.load).toFixed(2)+' <span class="unit">kg</span>';
  $('loadState').textContent=x.loadState;$('mode').textContent=x.auto?'AUTO':'MANUAL';
  $('autoBtn').textContent='AUTO: '+(x.auto?'ON':'OFF');
  $('streamBtn').textContent='STREAM: '+(x.stream?'ON':'OFF');stream=x.stream;
  $('scr').textContent=x.screen;$('sensor').textContent=x.adxl?'READY':'NOT FOUND';
  $('buzz').textContent=x.buzzer?'ON':'OFF';$('rgb').textContent=x.rgb;
  $('ll1').textContent='Load: '+Number(x.load).toFixed(2)+' kg';
  $('ll2').textContent=x.overallState||x.loadState;
  $('al1').textContent='RMS:'+Number(x.vibRms||0).toFixed(2)+'g';
  $('al2').textContent=(x.vibState||'NORMAL')+' P:'+Number(x.vibPeak||0).toFixed(2);
}
// Fetch newly buffered ADXL345 samples from the ESP32 and append them to the chart.
async function sample(){
  if(dataBusy || pollingPaused) return;
  dataBusy=true;
  try{
    const r=await fetch('/api/data?since='+encodeURIComponent(lastSeq),{cache:'no-store'});
    if(!r.ok) throw new Error('HTTP '+r.status);
    const payload=await r.json();
    const latest=Number(payload.latest||0);

    // Handle an ESP32 reboot, where its sample sequence starts again at zero.
    if(latest<lastSeq){lastSeq=0;pts=[];}

    if(Array.isArray(payload.samples)){
      for(const row of payload.samples){
        if(!Array.isArray(row)||row.length<5) continue;
        const seq=Number(row[0]);
        if(!Number.isFinite(seq)||seq<=lastSeq) continue;
        pts.push({seq:seq,ax:Number(row[1]),ay:Number(row[2]),az:Number(row[3]),mag:Number(row[4])});
        lastSeq=seq;
      }
      // Keep a rolling view so the graph remains responsive over long runs.
      if(pts.length>240) pts.splice(0,pts.length-240);
      draw();
    }
  }catch(e){
    // Status polling reports connection errors separately; keep the last plotted data.
  }finally{
    dataBusy=false;
  }
}

function draw(){let c=$('chart'),ctx=c.getContext('2d'),dpr=devicePixelRatio||1,w=c.clientWidth,h=c.clientHeight;c.width=Math.round(w*dpr);c.height=Math.round(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);let l=38,r=8,t=10,b=22,pw=w-l-r,ph=h-t-b,vals=pts.flatMap(p=>[p.ax,p.ay,p.az,p.mag]),mn=Math.min(-1.2,...vals),mx=Math.max(1.2,...vals),span=mx-mn;mn-=span*.08;mx+=span*.08;ctx.font='11px system-ui';ctx.strokeStyle='#2b3b58';ctx.fillStyle='#9eacc8';for(let i=0;i<=4;i++){let y=t+ph*i/4;ctx.beginPath();ctx.moveTo(l,y);ctx.lineTo(w-r,y);ctx.stroke();ctx.fillText((mx-(mx-mn)*i/4).toFixed(1),2,y+4)}[['ax','#70a5ff'],['ay','#35d39a'],['az','#ffca62'],['mag','#e5a2ff']].forEach(([k,col])=>{ctx.beginPath();ctx.strokeStyle=col;ctx.lineWidth=1.7;pts.forEach((p,i)=>{let x=l+(pts.length<2?0:i/(pts.length-1))*pw,y=t+(mx-p[k])/(mx-mn)*ph;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)});ctx.stroke()});if(!pts.length){ctx.fillStyle='#9eacc8';ctx.fillText('Waiting for sensor data…',l+8,t+20)}}
setInterval(refresh,1200);
setInterval(sample,100);
onresize=draw;refresh();sample();draw();
</script></body></html>
)HTML";

// ---------------- Outputs ----------------
void setLED(int pin, bool on) { digitalWrite(pin, on ? LOW : HIGH); } // common-anode
void setRGB(bool r,bool g,bool b){setLED(LED_RED,r);setLED(LED_GREEN,g);setLED(LED_BLUE,b);}
void allLEDsOff(){setRGB(false,false,false);}
void applyManualLEDs(){setRGB(manualR,manualG,manualB);}
void setBuzzer(bool on){buzzerOn=on;digitalWrite(BUZZER_PIN,on?HIGH:LOW);}

// ---------------- ADXL345 ----------------
bool writeReg(uint8_t reg,uint8_t val){Wire.beginTransmission(ADXL_ADDR);Wire.write(reg);Wire.write(val);return Wire.endTransmission()==0;}
uint8_t readReg(uint8_t reg){Wire.beginTransmission(ADXL_ADDR);Wire.write(reg);if(Wire.endTransmission(false)!=0)return 0xFF;if(Wire.requestFrom((uint8_t)ADXL_ADDR,(uint8_t)1)!=1)return 0xFF;return Wire.read();}

bool initADXL() {
  // Retry the device-ID read because the sensor may power up slightly later
  // than the ESP32. Keep the LCD and ADXL on the same I2C bus.
  for (uint8_t attempt = 1; attempt <= 10; ++attempt) {
    uint8_t id = readReg(REG_DEVID);

    if (id == 0xE5) {
      bool ok = writeReg(REG_DATA_FORMAT, 0x08); // Full resolution, +/-2g
      ok = writeReg(REG_BW_RATE, 0x0B) && ok;    // 200 Hz output rate
      ok = writeReg(REG_POWER_CTL, 0x08) && ok;  // Measurement mode

      if (ok) {
        delay(10);
        uint8_t verifyId = readReg(REG_DEVID);
        if (verifyId == 0xE5) {
          consecutiveAdxlErrors = 0;
          Serial.println("ADXL345 initialized (ID 0xE5).");
          return true;
        }
      }
    }

    delay(100);
  }

  Serial.println("ADXL345 not responding; will retry in the background.");
  return false;
}
bool readXYZ(int16_t &x,int16_t &y,int16_t &z){
  Wire.beginTransmission(ADXL_ADDR);Wire.write(REG_DATAX0);if(Wire.endTransmission(false)!=0)return false;
  if(Wire.requestFrom((uint8_t)ADXL_ADDR,(uint8_t)6)!=6)return false;
  uint8_t x0=Wire.read(),x1=Wire.read(),y0=Wire.read(),y1=Wire.read(),z0=Wire.read(),z1=Wire.read();
  x=(int16_t)((uint16_t)x1<<8|x0);y=(int16_t)((uint16_t)y1<<8|y0);z=(int16_t)((uint16_t)z1<<8|z0);return true;
}
void storeSample() {
  PlotSample &p = sampleBuffer[sampleHead];
  p.seq = sampleCount;
  p.x = ax; p.y = ay; p.z = az; p.magnitude = mag;
  sampleHead = (sampleHead + 1) % SAMPLE_BUFFER_SIZE;
  if (sampleCountBuffered < SAMPLE_BUFFER_SIZE) sampleCountBuffered++;
}

void emitSerialPlot() {
  if (!streamEnabled || millis() - lastSerialPlotMs < SERIAL_PLOT_INTERVAL_MS) return;
  lastSerialPlotMs = millis();
  // Arduino Serial Plotter recognises label:value pairs as distinct plotted series.
  Serial.print("X:"); Serial.print(ax, 3);
  Serial.print(",Y:"); Serial.print(ay, 3);
  Serial.print(",Z:"); Serial.print(az, 3);
  Serial.print(",MAG:"); Serial.println(mag, 3);
}

uint8_t loadSeverity(float kg);
void applyAuto();

void recomputeOverallAlert() {
  uint8_t loadLevel=loadSeverity(loadKg);
  uint8_t vibLevel=vibrationSeverity;
  uint8_t newLevel=loadLevel>vibLevel?loadLevel:vibLevel;

  String reason="Bridge status normal";
  if(loadLevel>0 && loadLevel>=vibLevel) {
    reason=loadLevel==2 ? "Critical load: "+String(loadKg,2)+" kg exceeds 5 kg."
                        : "Load warning: "+String(loadKg,2)+" kg is in the 4-5 kg warning band.";
  } else if(vibLevel==2) {
    reason="Critical vibration: "+vibrationReason+". Peak "+String(vibPeak,3)+" g; RMS "+String(vibRms,3)+" g.";
  } else if(vibLevel==1) {
    reason="Sustained vibration warning. RMS "+String(vibRms,3)+" g exceeded the warning threshold.";
  }

  // Determine new events per source, not just changes to overallSeverity.
  // Example: load is already CRITICAL (overallSeverity=2), then vibration
  // independently becomes CRITICAL. That second event must still make a toast.
  bool newLoadEvent = loadLevel > lastObservedLoadSeverity;
  bool newVibrationEvent = vibLevel > lastObservedVibrationSeverity;
  String eventText="";
  uint8_t eventLevel=0;

  if(newVibrationEvent && (!newLoadEvent || vibLevel >= loadLevel)) {
    eventLevel=vibLevel;
    if(vibLevel==2) {
      eventText="Critical vibration: "+vibrationReason+". Peak "+String(vibPeak,3)+" g; RMS "+String(vibRms,3)+" g.";
    } else {
      eventText="Sustained vibration warning. RMS "+String(vibRms,3)+" g exceeded the warning threshold.";
    }
  } else if(newLoadEvent) {
    eventLevel=loadLevel;
    eventText=loadLevel==2 ? "Critical load: "+String(loadKg,2)+" kg exceeds 5 kg."
                           : "Load warning: "+String(loadKg,2)+" kg is in the 4-5 kg warning band.";
  }

  lastObservedLoadSeverity=loadLevel;
  lastObservedVibrationSeverity=vibLevel;
  overallSeverity=newLevel;
  overallReason=reason;

  if(!autoMode)return;

  if(eventLevel>0) {
    alertLevel=eventLevel;
    alertSeq++;
    alertText=eventText;
    Serial.print("ALERT: "); Serial.println(eventText);
  } else if(newLevel==0 && alertLevel!=0) {
    alertLevel=0;
    alertText="Bridge condition returned to normal.";
  }
}

void updateVibrationDetector(float x,float y,float z) {
  unsigned long now=millis();
  if(!filterInitialized) {
    lpX=x; lpY=y; lpZ=z; filterInitialized=true;
    peakWindowStarted=now;
    previousFilteredV=0;
    return;
  }

  // High-pass filter removes gravity and slow changes in sensor orientation.
  lpX+=HP_ALPHA*(x-lpX); lpY+=HP_ALPHA*(y-lpY); lpZ+=HP_ALPHA*(z-lpZ);
  hpX=x-lpX; hpY=y-lpY; hpZ=z-lpZ;
  float v=sqrtf(hpX*hpX+hpY*hpY+hpZ*hpZ);

  // Rolling 1-second RMS (200 samples at 200 Hz).
  if(vibWindowCount==VIB_WINDOW) vibSquareSum-=vibSquares[vibWindowHead];
  else vibWindowCount++;
  vibSquares[vibWindowHead]=v*v;
  vibSquareSum+=v*v;
  vibWindowHead=(vibWindowHead+1)%VIB_WINDOW;
  vibRms=sqrtf(vibSquareSum/(float)vibWindowCount);

  // Maximum filtered acceleration observed during the current 1-second display window.
  if(now-peakWindowStarted>=1000UL) {
    vibPeak=v;
    peakWindowStarted=now;
  } else if(v>vibPeak) {
    vibPeak=v;
  }

  // Count distinct local peaks above the impact threshold, not every sample above it.
  // A short minimum spacing prevents one noisy peak from being counted repeatedly.
  bool localPeak=(previousFilteredV>v && previousFilteredV>=VIB_IMPACT_PEAK);
  if(localPeak && (lastVibPeakAt==0 || now-lastVibPeakAt>=VIB_MIN_PEAK_SPACING_MS)) {
    lastVibPeakAt=now;
    vibPeakTimes[vibPeakHead]=now;
    vibPeakHead=(vibPeakHead+1)%10;
    if(vibPeakStored<10) vibPeakStored++;
  }
  previousFilteredV=v;

  // Count only peaks from the last rolling 3 seconds.
  vibPeaksInWindow=0;
  for(uint8_t i=0;i<vibPeakStored;i++) {
    if(now-vibPeakTimes[i]<=VIB_PEAK_WINDOW_MS) vibPeaksInWindow++;
  }

  bool rmsReady=(vibWindowCount==VIB_WINDOW);
  if(rmsReady && vibRms>=VIB_WARN_RMS) {
    if(!vibWarnSince) vibWarnSince=now;
  } else {
    vibWarnSince=0;
  }

  uint8_t previous=vibrationSeverity;
  bool repeatedBanging=(vibPeaksInWindow>=VIB_PEAKS_FOR_CRITICAL);
  bool warningSustained=(vibWarnSince && now-vibWarnSince>=VIB_SUSTAINED_MS);

  // Critical requires repeated banging: at least four distinct peaks in three seconds.
  // A steady elevated RMS alone produces a warning after three seconds.
  if(repeatedBanging) {
    vibrationSeverity=2;
    vibrationReason="4 or more vibration peaks in 3 seconds";
  } else if(vibrationSeverity<2 && warningSustained) {
    vibrationSeverity=1;
    vibrationReason="vibration RMS sustained for 3 seconds";
  }

  // Clear an alert only after vibration has settled and the 3-second peak window is quiet.
  float clearLimit=(vibrationSeverity==2)?VIB_CRIT_CLEAR_RMS:VIB_WARN_CLEAR_RMS;
  bool peakWindowQuiet=(vibPeaksInWindow==0) &&
      (lastVibPeakAt==0 || now-lastVibPeakAt>VIB_PEAK_WINDOW_MS);
  if(vibrationSeverity>0 && rmsReady && vibRms<clearLimit && peakWindowQuiet) {
    if(!vibClearSince) vibClearSince=now;
    if(now-vibClearSince>=1000UL) {
      vibrationSeverity=0;
      vibrationReason="normal";
      vibClearSince=0;
      vibWarnSince=0;
    }
  } else {
    vibClearSince=0;
  }

  if(previous!=vibrationSeverity) {
    Serial.print("Vibration state: ");
    Serial.println(vibrationSeverity==2?"CRITICAL":vibrationSeverity==1?"WARNING":"NORMAL");
  }
  recomputeOverallAlert();
  if(autoMode) applyAuto();
}

void updateADXL() {
  if(!adxlReady)return;
  int16_t x,y,z;
  if(!readXYZ(x,y,z)){
    if(++consecutiveAdxlErrors>=5){
      adxlReady=false;lastAdxlRetry=millis();
      Serial.println("ADXL345 read failed repeatedly; retrying initialization.");
    }
    return;
  }
  consecutiveAdxlErrors=0;
  ax=x*G_PER_LSB;ay=y*G_PER_LSB;az=z*G_PER_LSB;
  mag=sqrtf(ax*ax+ay*ay+az*az);
  updateVibrationDetector(ax,ay,az);
  sampleCount++;lastSampleMs=millis();storeSample();emitSerialPlot();
}// ---------------- Load, LCD, automatic outputs ----------------
const char* loadState(){if(loadKg>DANGER_KG)return "OVERLOAD";if(loadKg>=WARN_KG)return "WARNING";return "NORMAL";}
void applyAuto(){if(!autoMode)return;if(overallSeverity>=2)setRGB(true,false,false);else if(overallSeverity==1)setRGB(true,true,false);else setRGB(false,true,false);}
void updateBuzzer(){
  if(!autoMode) return;
  unsigned long n=millis();
  if(overallSeverity>=1) {
    // Both warning and critical use the original short beep every second.
    if(buzzerOn) {
      if(n-beepStarted>=BEEP_MS) setBuzzer(false);
      return;
    }
    if(beepStarted==0 || n-beepStarted>=BEEP_EVERY) {
      setBuzzer(true);
      beepStarted=n;
    }
    return;
  }
  if(buzzerOn) setBuzzer(false);
  beepStarted=0;
}

void lcdLoad(){lcd.setCursor(0,0);lcd.print("Load: ");lcd.print(loadKg,2);lcd.print(" kg   ");lcd.setCursor(0,1);if(overallSeverity==2)lcd.print("CRITICAL ALERT  ");else if(overallSeverity==1)lcd.print("WARNING ACTIVE  ");else lcd.print("NORMAL          ");}
void lcdADXL(){
  if(!adxlReady){lcd.setCursor(0,0);lcd.print("ADXL NOT FOUND  ");lcd.setCursor(0,1);lcd.print("Check I2C/power ");return;}
  if(vibrationSeverity==2){lcd.setCursor(0,0);lcd.print("VIB CRITICAL    ");lcd.setCursor(0,1);lcd.print("P:");lcd.print(vibPeak,2);lcd.print(" RMS:");lcd.print(vibRms,2);lcd.print(" ");return;}
  if(vibrationSeverity==1){lcd.setCursor(0,0);lcd.print("VIB WARNING     ");lcd.setCursor(0,1);lcd.print("RMS:");lcd.print(vibRms,3);lcd.print(" g       ");return;}
  lcd.setCursor(0,0);lcd.print("RMS:");lcd.print(vibRms,3);lcd.print(" P:");lcd.print(vibPeak,2);lcd.print(" ");
  lcd.setCursor(0,1);lcd.print("X:");lcd.print(ax,1);lcd.print(" Y:");lcd.print(ay,1);lcd.print("    ");
}
void updateLCD(){if(screenNow==LOAD_SCREEN)lcdLoad();else lcdADXL();}
void switchScreen(Screen s){screenNow=s;lcd.clear();updateLCD();Serial.println(s==LOAD_SCREEN?"LCD screen: LOAD":"LCD screen: ADXL");}

// ---------------- Status API ----------------
String statusJSON(){
  String rgb;if(autoMode)rgb=overallSeverity>=2?"RED":overallSeverity==1?"YELLOW":"GREEN";else{if(manualR)rgb+="R";if(manualG)rgb+="G";if(manualB)rgb+="B";if(!rgb.length())rgb="OFF";}
  String j="{\"wifi\":";j+=WiFi.status()==WL_CONNECTED?"true":"false";j+=",\"ip\":\""+WiFi.localIP().toString()+"\"";
  j+=",\"adxl\":";j+=adxlReady?"true":"false";j+=",\"ax\":"+String(ax,4)+",\"ay\":"+String(ay,4)+",\"az\":"+String(az,4)+",\"mag\":"+String(mag,4);
  j+=",\"vibRms\":"+String(vibRms,4)+",\"vibPeak\":"+String(vibPeak,4)+",\"vibState\":\""+String(vibrationSeverity==2?"CRITICAL":vibrationSeverity==1?"WARNING":"NORMAL")+"\"";
  j+=",\"overallState\":\""+String(overallSeverity==2?"CRITICAL":overallSeverity==1?"WARNING":"NORMAL")+"\"";
  j+=",\"load\":"+String(loadKg,2)+",\"loadState\":\""+String(loadState())+"\",\"auto\":";j+=autoMode?"true":"false";
  j+=",\"stream\":";j+=streamEnabled?"true":"false";j+=",\"buzzer\":";j+=buzzerOn?"true":"false";
  j+=",\"screen\":\""+String(screenNow==LOAD_SCREEN?"LOAD":"ADXL")+"\",\"rgb\":\""+rgb+"\",\"samples\":"+String(sampleCount)+",\"sampleAgeMs\":"+String(lastSampleMs?millis()-lastSampleMs:0);
  String safeReason=alertText; safeReason.replace("\\"," "); safeReason.replace("\"","'");
  j+=",\"alertSeq\":"+String(alertSeq)+",\"alertLevel\":"+String(alertLevel)+",\"alertText\":\""+safeReason+"\"}";
  return j;
}
void handleStatus(){server.sendHeader("Cache-Control","no-store");server.sendHeader("Connection","close");server.send(200,"application/json",statusJSON());}
void handleData() {
  uint32_t since = 0;
  if (server.hasArg("since")) since = (uint32_t)strtoul(server.arg("since").c_str(), nullptr, 10);
  String json = "{\"latest\":" + String(sampleCount) + ",\"samples\":[";
  bool first = true;
  uint16_t oldest = (sampleHead + SAMPLE_BUFFER_SIZE - sampleCountBuffered) % SAMPLE_BUFFER_SIZE;
  uint16_t sent = 0;
  for (uint16_t i = 0; i < sampleCountBuffered && sent < 80; ++i) {
    const PlotSample &p = sampleBuffer[(oldest + i) % SAMPLE_BUFFER_SIZE];
    if ((int32_t)(p.seq - since) <= 0) continue;
    if (!first) json += ',';
    first = false;
    json += "[" + String(p.seq) + "," + String(p.x, 4) + "," + String(p.y, 4) + "," +
            String(p.z, 4) + "," + String(p.magnitude, 4) + "]";
    ++sent;
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", json);
}
void processCommand(char* c);

void handleCommand(){
  String c=server.arg("plain");c.trim();if(!c.length()||c.length()>60){server.send(400,"text/plain","Invalid command");return;}
  char buf[64];c.toCharArray(buf,sizeof(buf));processCommand(buf);server.sendHeader("Connection","close");server.send(200,"text/plain","Accepted: "+c);
}
void startServer(){server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",PAGE);});server.on("/api/status",HTTP_GET,handleStatus);server.on("/api/data",HTTP_GET,handleData);server.on("/api/cmd",HTTP_POST,handleCommand);server.onNotFound([]{server.send(404,"text/plain","Not found");});server.begin();serverStarted = true;Serial.println("HTTP server started.");}

// ---------------- Wi-Fi ----------------

bool tryWiFi(const char* ssid, const char* pass) {
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Stop any previous connection attempt before changing credentials.
  WiFi.disconnect(true, false);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOST);
  delay(200);

  WiFi.begin(ssid, pass);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 12000UL) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print("Failed to connect to ");
  Serial.println(ssid);

  // Stop this attempt cleanly before trying another network.
  WiFi.disconnect(true, false);
  delay(500);

  return false;
}


void setupNetwork() {
  wifiReady = false;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOST);
  WiFi.setAutoReconnect(false);

  wifiReady = tryWiFi(WIFI1, PASS1);

  if (!wifiReady) {
    wifiReady = tryWiFi(WIFI2, PASS2);
  }

  if (wifiReady) {
    if (!mdnsStarted) {
      if (MDNS.begin(HOST)) {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
        Serial.println("Dashboard: http://dashboard.local");
      } else {
        Serial.println("mDNS failed; use the printed IP address.");
      }
    }

    if (!serverStarted) {
      startServer();
    }
  } else {
    Serial.println("Wi-Fi unavailable; will retry automatically.");
  }

  // Start the retry interval after this (potentially long) connection attempt.
  lastWifiRetry = millis();
}


// ---------------- Serial command parser ----------------
void help(){Serial.println("Vibration: warning RMS >=0.040g for 3s; critical >=4 filtered peaks >=0.300g in rolling 3s. Prototype thresholds; calibrate on model.");Serial.println("SCREEN LOAD | SCREEN ADXL | SCREEN");Serial.println("LOAD 2.50 | AUTO ON | AUTO OFF");Serial.println("R ON/OFF | G ON/OFF | B ON/OFF | LED OFF | RGB r g b");Serial.println("BUZZER ON/OFF | STREAM ON/OFF | PLOT ON/OFF | STATUS | HELP");}

uint8_t loadSeverity(float kg) {
  if (kg > DANGER_KG) return 2;
  if (kg >= WARN_KG) return 1;
  return 0;
}

void updateLoadAlert(float previousLoad, float newLoad) {
  (void)previousLoad; (void)newLoad;
  recomputeOverallAlert();
  if(autoMode)applyAuto();
}

void processCommand(char* c){
  for(char*p=c;*p;p++)if(*p>='a'&&*p<='z')*p-=32;
  if(!strcmp(c,"HELP")){help();return;}
  if(!strcmp(c,"SCREEN LOAD")){switchScreen(LOAD_SCREEN);return;}
  if(!strcmp(c,"SCREEN ADXL")){switchScreen(ADXL_SCREEN);return;}
  if(!strcmp(c,"SCREEN")){Serial.println(screenNow==LOAD_SCREEN?"LOAD":"ADXL");return;}
  if(!strcmp(c,"STREAM ON")||!strcmp(c,"PLOT ON")){streamEnabled=true;Serial.println("ADXL serial stream ON");return;}
  if(!strcmp(c,"STREAM OFF")||!strcmp(c,"PLOT OFF")){streamEnabled=false;Serial.println("ADXL serial stream OFF");return;}
  if(!strcmp(c,"AUTO ON")){bool wasAuto=autoMode;uint32_t seqBefore=alertSeq;autoMode=true;beepStarted=0;recomputeOverallAlert();if(!wasAuto&&overallSeverity>0&&alertSeq==seqBefore){alertLevel=overallSeverity;alertSeq++;alertText=overallReason;}applyAuto();Serial.println("AUTO ON");return;}
  if(!strcmp(c,"AUTO OFF")){autoMode=false;allLEDsOff();setBuzzer(false);manualR=manualG=manualB=false;Serial.println("AUTO OFF; manual controls enabled");return;}
  if(!strncmp(c,"LOAD ",5)){char*e;float n=strtof(c+5,&e);if(e==c+5||*e||!isfinite(n)||n<0||n>999.99){Serial.println("Invalid load: use LOAD 0 to LOAD 999.99");return;}float previousLoad=loadKg;loadKg=n;updateLoadAlert(previousLoad,loadKg);if(autoMode)applyAuto();if(screenNow==LOAD_SCREEN)updateLCD();Serial.print("Simulated load: ");Serial.print(loadKg,2);Serial.print(" kg / ");Serial.println(loadState());return;}
  if(!strcmp(c,"STATUS")){Serial.println(statusJSON());return;}
  if(autoMode){if(!strcmp(c,"R ON")||!strcmp(c,"R OFF")||!strcmp(c,"G ON")||!strcmp(c,"G OFF")||!strcmp(c,"B ON")||!strcmp(c,"B OFF")||!strcmp(c,"LED OFF")||!strncmp(c,"RGB ",4)||!strcmp(c,"BUZZER ON")||!strcmp(c,"BUZZER OFF")){Serial.println("AUTO mode active; send AUTO OFF first.");return;}}
  else{
    if(!strcmp(c,"R ON")){manualR=true;manualG=manualB=false;applyManualLEDs();return;}
    if(!strcmp(c,"G ON")){manualG=true;manualR=manualB=false;applyManualLEDs();return;}
    if(!strcmp(c,"B ON")){manualB=true;manualR=manualG=false;applyManualLEDs();return;}
    if(!strcmp(c,"R OFF")){manualR=false;applyManualLEDs();return;}
    if(!strcmp(c,"G OFF")){manualG=false;applyManualLEDs();return;}
    if(!strcmp(c,"B OFF")){manualB=false;applyManualLEDs();return;}
    if(!strcmp(c,"LED OFF")){manualR=manualG=manualB=false;allLEDsOff();return;}
    if(!strncmp(c,"RGB ",4)){int r,g,b;char extra;if(sscanf(c+4,"%d %d %d %c",&r,&g,&b,&extra)==3){manualR=constrain(r,0,255)>0;manualG=constrain(g,0,255)>0;manualB=constrain(b,0,255)>0;applyManualLEDs();}else Serial.println("Use RGB 255 0 0");return;}
    if(!strcmp(c,"BUZZER ON")){setBuzzer(true);return;}if(!strcmp(c,"BUZZER OFF")){setBuzzer(false);return;}
  }
  Serial.println("Unknown command. Enter HELP.");
}
void readSerial(){while(Serial.available()){char c=Serial.read();if(c=='\r')continue;if(c=='\n'){cmdBuffer[cmdLen]='\0';if(cmdLen)processCommand(cmdBuffer);cmdLen=0;}else if(cmdLen<sizeof(cmdBuffer)-1)cmdBuffer[cmdLen++]=c;else{cmdLen=0;Serial.println("Command too long");}}}

// ---------------- Setup / loop ----------------
void setup(){
  digitalWrite(BUZZER_PIN,LOW);pinMode(BUZZER_PIN,OUTPUT);setBuzzer(false);
  pinMode(LED_RED,OUTPUT);pinMode(LED_GREEN,OUTPUT);pinMode(LED_BLUE,OUTPUT);allLEDsOff();
  Serial.begin(115200);delay(200);
  Wire.begin(I2C_SDA,I2C_SCL);Wire.setClock(100000);
  lcd.init();lcd.backlight();lcd.clear();lcd.setCursor(0,0);lcd.print("Bridge Monitor");lcd.setCursor(0,1);lcd.print("Starting...");delay(500);
  adxlReady = initADXL();
  lastAdxlRetry = millis();
  screenNow=LOAD_SCREEN;loadKg=0;autoMode=true;applyAuto();lcd.clear();updateLCD();nextSample=micros();lastLCD=millis();
  Serial.println("\nBridge Monitor Dashboard");Serial.print("ADXL345: ");Serial.println(adxlReady?"READY":"NOT FOUND");Serial.println("Load is simulated; HX711 is not read.");
  setupNetwork();Serial.println("Type HELP for commands.");
}
void loop(){
  readSerial();
  uint32_t now = micros();
  if (adxlReady && (int32_t)(now - nextSample) >= 0) {
    nextSample += SAMPLE_US;
    if ((int32_t)(micros() - nextSample) > 20000) {
      nextSample = micros() + SAMPLE_US;
    }
    updateADXL();
  }

  // If the sensor was absent at boot or later stopped responding, retry it.
  if (!adxlReady && millis() - lastAdxlRetry >= 2000UL) {
    lastAdxlRetry = millis();
    adxlReady = initADXL();
    if (adxlReady) {
      nextSample = micros();
      Serial.println("ADXL345 recovered.");
      if (screenNow == ADXL_SCREEN) {
        lcd.clear();
        updateLCD();
      }
    }
  }

  updateBuzzer();
  if (millis() - lastLCD >= 250) {
    lastLCD = millis();
    updateLCD();
  }
  
if (WiFi.status() == WL_CONNECTED) {
  if (!wifiReady) {
    wifiReady = true;

    Serial.print("Wi-Fi reconnected. IP: ");
    Serial.println(WiFi.localIP());

    if (!mdnsStarted) {
      if (MDNS.begin(HOST)) {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
        Serial.println("Dashboard: http://dashboard.local");
      }
    }

    if (!serverStarted) {
      startServer();
    }
  }

  if (serverStarted) {
    server.handleClient();
  }
} else {
  if (wifiReady) {
    wifiReady = false;
    Serial.println("Wi-Fi disconnected.");
    if (mdnsStarted) {
      MDNS.end();
      mdnsStarted = false;
    }
  }

  // Retry both configured networks every 15 seconds.
  if (millis() - lastWifiRetry >= 15000UL) {
    lastWifiRetry = millis();
    setupNetwork();
  }
}

}
