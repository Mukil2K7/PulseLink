#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ─── WiFi Config ──────────────────────────────────────────────────────────────
const char* ssid     = "Redmi";
const char* password = "mukil2k7";

// ─── CallMeBot WhatsApp Config ────────────────────────────────────────────────
// 1. Save +34 691 62 42 00 on WhatsApp
// 2. Send: "I allow callmebot to send me messages"
// 3. You will receive your API key - paste below
const String phoneNumber = "+91XXXXXXXXXX";
const String apiKey      = "XXXXXX";

// ─── Pulse Sensor ─────────────────────────────────────────────────────────────
#define PULSE_PIN        34
#define FINGER_THRESHOLD 1200
#define BEAT_THRESHOLD   2048

WebServer server(80);

// ─── BPM State ────────────────────────────────────────────────────────────────
int  bpm           = 0;
int  minBpm        = 999;
int  maxBpm        = 0;
long totalBpm      = 0;
int  bpmCount      = 0;
bool fingerPresent = false;
unsigned long lastBeatTime = 0;

bool abnormalAlertSent = false;
unsigned long abnormalSince = 0;
#define ABNORMAL_ALERT_DELAY 10000

#define SAMPLE_SIZE 5
int bpmSamples[SAMPLE_SIZE] = {0};
int sampleIndex = 0;

#define HISTORY_SIZE 20
struct BpmRecord { int bpm; unsigned long ts; };
BpmRecord history[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;

void addHistory(int b) {
  history[historyIndex] = { b, millis() / 1000 };
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) historyCount++;
  totalBpm += b; bpmCount++;
  if (b < minBpm) minBpm = b;
  if (b > maxBpm) maxBpm = b;
}

void sendWhatsApp(String message) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&text=" + message + "&apikey=" + apiKey;
  http.begin(url);
  int code = http.GET();
  Serial.println(code > 0 ? "WhatsApp sent! Code: " + String(code) : "WhatsApp failed: " + String(code));
  http.end();
}

void handleBPM() {
  int avg = bpmCount > 0 ? (int)(totalBpm / bpmCount) : 0;
  String json = "{";
  json += "\"bpm\":"    + String(bpm) + ",";
  json += "\"finger\":" + String(fingerPresent ? "true" : "false") + ",";
  json += "\"min\":"    + String(minBpm == 999 ? 0 : minBpm) + ",";
  json += "\"max\":"    + String(maxBpm) + ",";
  json += "\"avg\":"    + String(avg) + ",";
  json += "\"history\":[";
  int start = historyCount < HISTORY_SIZE ? 0 : historyIndex;
  for (int i = 0; i < historyCount; i++) {
    int idx = (start + i) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += "{\"bpm\":" + String(history[idx].bpm) + ",\"ts\":" + String(history[idx].ts) + "}";
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleSOS() {
  String currentBpm = server.arg("bpm");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "SOS sent!");
  String msg = "SOS+EMERGENCY+ALERT!+Patient+needs+help!+Heart+Rate:+" + currentBpm + "+BPM+Please+respond+immediately!";
  sendWhatsApp(msg);
  Serial.println("Manual SOS! BPM: " + currentBpm);
}

void handleRoot() {
  String h = "";
  h += "<!DOCTYPE html><html lang='en'><head>";
  h += "<meta charset='UTF-8'>";
  h += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>PulseLink Pro</title>";
  h += "<link href='https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap' rel='stylesheet'>";
  h += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css'>";
  h += "<script src='https://cdn.tailwindcss.com'></script>";
  h += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  h += "<style>";
  h += "body{background-color:#0f172a;background-image:radial-gradient(at 0% 0%,hsla(253,16%,7%,1) 0,transparent 50%),radial-gradient(at 50% 0%,hsla(225,39%,30%,1) 0,transparent 50%),radial-gradient(at 100% 0%,hsla(339,49%,30%,1) 0,transparent 50%);color:white;min-height:100vh;}";
  h += ".gc{background:rgba(255,255,255,0.03);backdrop-filter:blur(16px);border:1px solid rgba(255,255,255,0.1);border-radius:20px;}";
  h += ".ni.active{background:rgba(0,242,234,0.1);border-left:3px solid #00f2ea;color:#00f2ea;}";
  h += ".mn.active{color:#00f2ea;}";
  h += ".pa{animation:pr 2s infinite;}";
  h += "@keyframes pr{0%{box-shadow:0 0 0 0 rgba(255,0,85,0.4)}70%{box-shadow:0 0 0 20px rgba(255,0,85,0)}100%{box-shadow:0 0 0 0 rgba(255,0,85,0)}}";
  h += "::-webkit-scrollbar{width:6px}::-webkit-scrollbar-track{background:#0f172a}::-webkit-scrollbar-thumb{background:#334155;border-radius:10px}";
  h += ".toast{position:fixed;top:24px;right:24px;z-index:9999;padding:14px 24px;border-radius:14px;font-weight:600;font-size:15px;display:none;}";
  h += "</style></head>";
  h += "<body class='flex overflow-hidden'>";

  h += "<div id='toast' class='toast bg-red-600 text-white shadow-lg'>";
  h += "<i class='fa-solid fa-triangle-exclamation mr-2'></i><span id='toast-msg'>Alert!</span></div>";

  h += "<aside class='w-64 gc m-4 mr-0 flex flex-col h-[95vh] sticky top-4 hidden md:flex'>";
  h += "<div class='p-8 text-2xl font-bold tracking-wider text-cyan-400'><i class='fa-solid fa-heart-pulse mr-2'></i> PULSELINK PRO</div>";
  h += "<nav class='flex-1 flex flex-col gap-2 px-4'>";
  h += "<button onclick=\"sp('dashboard')\" id='nav-dashboard' class='ni active flex items-center gap-4 px-4 py-4 rounded-xl hover:bg-white/5 transition text-gray-300'><i class='fa-solid fa-chart-line'></i> Dashboard</button>";
  h += "<button onclick=\"sp('history')\" id='nav-history' class='ni flex items-center gap-4 px-4 py-4 rounded-xl hover:bg-white/5 transition text-gray-300'><i class='fa-solid fa-clock-rotate-left'></i> History</button>";
  h += "<button onclick=\"sp('profile')\" id='nav-profile' class='ni flex items-center gap-4 px-4 py-4 rounded-xl hover:bg-white/5 transition text-gray-300'><i class='fa-solid fa-user'></i> Patient</button>";
  h += "<button onclick=\"sp('pharmacy')\" id='nav-pharmacy' class='ni flex items-center gap-4 px-4 py-4 rounded-xl hover:bg-white/5 transition text-gray-300'><i class='fa-solid fa-pills'></i> Pharmacy</button>";
  h += "<button onclick=\"sp('payment')\" id='nav-payment' class='ni flex items-center gap-4 px-4 py-4 rounded-xl hover:bg-white/5 transition text-gray-300'><i class='fa-solid fa-credit-card'></i> Payment</button>";
  h += "<button onclick=\"sp('emergency')\" id='nav-emergency' class='ni flex items-center gap-4 px-4 py-4 rounded-xl hover:bg-white/5 transition text-red-400'><i class='fa-solid fa-triangle-exclamation'></i> Emergency</button>";
  h += "</nav>";
  h += "<div class='p-6 border-t border-white/10'><p class='text-sm font-semibold' id='sname'>Patient</p><p class='text-xs text-gray-400'>Live Sensor</p><div class='mt-3 bg-white/5 rounded-xl p-3 text-center border border-white/10'><p class='text-[10px] text-gray-400 uppercase tracking-widest mb-1'>Local Time</p><p id='clock' class='text-2xl font-bold text-cyan-400 tracking-widest'>00:00:00</p><p id='clockdate' class='text-[10px] text-gray-500 mt-1'></p></div></div>";
  h += "</aside>";

  h += "<main class='flex-1 p-4 md:p-8 h-screen overflow-y-auto relative pb-24 md:pb-8'>";
  h += "<div class='fixed bottom-8 right-8 z-50 hidden md:flex'>";
  h += "<button onclick=\"sp('emergency')\" class='bg-red-500 hover:bg-red-600 text-white w-16 h-16 rounded-full shadow-lg transition flex items-center justify-center font-bold text-xl'>SOS</button></div>";

  h += "<section id='page-dashboard' class='space-y-6'>";
  h += "<header><h1 class='text-3xl font-bold'>Live Monitoring</h1><p class='text-gray-400'>Real-time sensor data</p></header>";
  h += "<div class='grid grid-cols-1 md:grid-cols-3 gap-6'>";
  h += "<div id='bpm-card' class='gc p-6 flex flex-col justify-between h-64 relative overflow-hidden transition-all duration-300'>";
  h += "<div class='absolute top-0 right-0 p-32 bg-cyan-400/10 blur-[50px] rounded-full'></div>";
  h += "<div><h3 class='text-gray-400 font-medium'>Heart Rate</h3>";
  h += "<div class='flex items-end gap-2 mt-2'><span id='bpm-display' class='text-6xl font-bold text-white'>--</span><span class='text-xl text-gray-400 mb-2'>BPM</span></div></div>";
  h += "<div class='flex items-center gap-2 text-sm mt-4'><span id='bstd' class='w-2 h-2 rounded-full bg-gray-400'></span><span id='bstt' class='text-gray-400 font-medium'>Waiting...</span></div>";
  h += "<i class='fa-solid fa-heart text-8xl absolute -bottom-4 -right-4 text-white/5'></i></div>";
  h += "<div class='gc p-6 col-span-2 flex flex-col'><h3 class='text-gray-400 font-medium mb-4'>ECG / Pulse History</h3>";
  h += "<div class='flex-1 w-full relative h-40'><canvas id='hrc'></canvas></div></div></div>";
  h += "<div class='grid grid-cols-3 gap-6'>";
  h += "<div class='gc p-5 text-center'><p class='text-gray-400 text-sm mb-1'>Min BPM</p><p id='smin' class='text-3xl font-bold text-cyan-400'>--</p></div>";
  h += "<div class='gc p-5 text-center'><p class='text-gray-400 text-sm mb-1'>Avg BPM</p><p id='savg' class='text-3xl font-bold text-white'>--</p></div>";
  h += "<div class='gc p-5 text-center'><p class='text-gray-400 text-sm mb-1'>Max BPM</p><p id='smax' class='text-3xl font-bold text-red-400'>--</p></div></div>";
  h += "<div class='grid grid-cols-2 md:grid-cols-4 gap-6'>";
  h += "<div class='gc p-4'><p class='text-gray-400 text-sm'>SpO2</p><p class='text-2xl font-bold mt-1'>98%</p></div>";
  h += "<div class='gc p-4'><p class='text-gray-400 text-sm'>Blood Pressure</p><p class='text-2xl font-bold mt-1'>120/80</p></div>";
  h += "<div class='gc p-4'><p class='text-gray-400 text-sm'>Temperature</p><p class='text-2xl font-bold mt-1'>36.6C</p></div>";
  h += "<div class='gc p-4'><p class='text-gray-400 text-sm'>Respiration</p><p class='text-2xl font-bold mt-1'>16 rpm</p></div></div>";
  h += "</section>";

  h += "<section id='page-history' class='hidden space-y-6'>";
  h += "<header class='flex justify-between items-center'><div><h1 class='text-3xl font-bold'>BPM History</h1><p class='text-gray-400'>Last 20 readings with timestamps</p></div>";
  h += "<button onclick='dlCSV()' class='flex items-center gap-2 bg-cyan-400 hover:scale-105 transition text-black font-bold px-5 py-3 rounded-xl shadow-lg'><i class='fa-solid fa-download'></i> Download CSV</button></header>";
  h += "<div class='gc p-6'><table class='w-full text-sm'>";
  h += "<thead><tr class='text-gray-400 border-b border-white/10 text-left'><th class='pb-3'>#</th><th class='pb-3'>BPM</th><th class='pb-3'>Status</th><th class='pb-3'>Time(s)</th></tr></thead>";
  h += "<tbody id='htbl'></tbody></table>";
  h += "<p id='hemp' class='text-gray-500 text-center mt-8'>No data yet.</p></div></section>";

  h += "<section id='page-profile' class='hidden space-y-6'>";
  h += "<header><h1 class='text-3xl font-bold'>Patient Record</h1></header>";
  h += "<div class='flex flex-col md:flex-row gap-6'>";
  h += "<div class='gc p-8 flex flex-col items-center w-full md:w-1/3 text-center gap-3'>";
  h += "<div class='w-24 h-24 rounded-full bg-cyan-400/10 flex items-center justify-center border-2 border-cyan-400'><i class='fa-solid fa-user text-4xl text-cyan-400'></i></div>";
  h += "<input id='pname' class='bg-white/5 rounded-lg px-3 py-2 text-center font-bold text-xl w-full border border-white/10 focus:outline-none' value='Patient Name'/>";
  h += "<input id='page' class='bg-white/5 rounded-lg px-3 py-2 text-center text-gray-400 w-full border border-white/10 focus:outline-none' value='Age, Gender'/>";
  h += "<div class='w-full space-y-2 mt-4 text-left'>";
  h += "<div class='flex justify-between text-sm py-2 border-b border-white/10'><span class='text-gray-400'>Blood Type</span><input id='pblood' class='bg-transparent text-right w-20 focus:outline-none' value='O+'/></div>";
  h += "<div class='flex justify-between text-sm py-2 border-b border-white/10'><span class='text-gray-400'>Height</span><input id='pheight' class='bg-transparent text-right w-20 focus:outline-none' value='-- cm'/></div>";
  h += "<div class='flex justify-between text-sm py-2 border-b border-white/10'><span class='text-gray-400'>Weight</span><input id='pweight' class='bg-transparent text-right w-20 focus:outline-none' value='-- kg'/></div>";
  h += "<div class='flex justify-between text-sm py-2 border-b border-white/10'><span class='text-gray-400'>BPM</span><span id='pbpm' class='text-cyan-400 font-bold'>--</span></div></div>";
  h += "<button onclick='savePro()' class='mt-4 w-full bg-cyan-400 text-black font-bold py-2 rounded-xl hover:scale-105 transition'>Save</button></div>";
  h += "<div class='gc p-8 w-full md:w-2/3'>";
  h += "<div class='flex justify-between items-center mb-4'><h3 class='text-xl font-bold'>Medical History</h3>";
  h += "<button onclick='addRec()' class='bg-white/10 hover:bg-cyan-400 hover:text-black px-4 py-2 rounded-xl text-sm transition font-semibold'>+ Add</button></div>";
  h += "<ul id='mrecs' class='space-y-4'>";
  h += "<li class='bg-white/5 p-4 rounded-lg border-l-4 border-yellow-500'><h4 class='font-bold text-sm'>Hairline Fracture</h4><p class='text-xs text-gray-400 mt-1'>Dec 2025 - Recovered</p></li>";
  h += "<li class='bg-white/5 p-4 rounded-lg border-l-4 border-blue-500'><h4 class='font-bold text-sm'>Annual Physical</h4><p class='text-xs text-gray-400 mt-1'>Jan 2026</p><p class='text-sm text-gray-300 mt-2'>All vitals normal.</p></li>";
  h += "</ul></div></div></section>";

  h += "<section id='page-pharmacy' class='hidden space-y-6'>";
  h += "<header><h1 class='text-3xl font-bold'>Pharmacy</h1><p class='text-gray-400'>Order medicines</p></header>";
  h += "<div class='flex flex-col lg:flex-row gap-6'><div class='flex-1 grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 content-start'>";
  h += "<div class='gc p-4 flex flex-col'><div class='bg-white/5 h-28 rounded-lg mb-4 flex items-center justify-center text-gray-500'><i class='fa-solid fa-pills text-4xl'></i></div><h3 class='font-bold'>Paracetamol</h3><p class='text-xs text-gray-400 mb-3'>500mg 10 Strips</p><div class='mt-auto flex justify-between items-center'><span class='text-cyan-400 font-bold'>Rs.40</span><button onclick=\"atc('Paracetamol',40)\" class='bg-white/10 hover:bg-cyan-400 hover:text-black w-8 h-8 rounded-full transition flex items-center justify-center'><i class='fa-solid fa-plus'></i></button></div></div>";
  h += "<div class='gc p-4 flex flex-col'><div class='bg-white/5 h-28 rounded-lg mb-4 flex items-center justify-center text-gray-500'><i class='fa-solid fa-prescription-bottle-medical text-4xl'></i></div><h3 class='font-bold'>Calcium Vit-D</h3><p class='text-xs text-gray-400 mb-3'>30 Tabs</p><div class='mt-auto flex justify-between items-center'><span class='text-cyan-400 font-bold'>Rs.350</span><button onclick=\"atc('Calcium Vit-D',350)\" class='bg-white/10 hover:bg-cyan-400 hover:text-black w-8 h-8 rounded-full transition flex items-center justify-center'><i class='fa-solid fa-plus'></i></button></div></div>";
  h += "<div class='gc p-4 flex flex-col'><div class='bg-white/5 h-28 rounded-lg mb-4 flex items-center justify-center text-gray-500'><i class='fa-solid fa-syringe text-4xl'></i></div><h3 class='font-bold'>Insulin Pen</h3><p class='text-xs text-gray-400 mb-3'>Disposable</p><div class='mt-auto flex justify-between items-center'><span class='text-cyan-400 font-bold'>Rs.800</span><button onclick=\"atc('Insulin Pen',800)\" class='bg-white/10 hover:bg-cyan-400 hover:text-black w-8 h-8 rounded-full transition flex items-center justify-center'><i class='fa-solid fa-plus'></i></button></div></div>";
  h += "<div class='gc p-4 flex flex-col'><div class='bg-white/5 h-28 rounded-lg mb-4 flex items-center justify-center text-gray-500'><i class='fa-solid fa-mask-face text-4xl'></i></div><h3 class='font-bold'>N95 Masks</h3><p class='text-xs text-gray-400 mb-3'>Pack of 5</p><div class='mt-auto flex justify-between items-center'><span class='text-cyan-400 font-bold'>Rs.250</span><button onclick=\"atc('N95 Masks',250)\" class='bg-white/10 hover:bg-cyan-400 hover:text-black w-8 h-8 rounded-full transition flex items-center justify-center'><i class='fa-solid fa-plus'></i></button></div></div>";
  h += "<div class='gc p-4 flex flex-col'><div class='bg-white/5 h-28 rounded-lg mb-4 flex items-center justify-center text-gray-500'><i class='fa-solid fa-heart-pulse text-4xl'></i></div><h3 class='font-bold'>BP Strip</h3><p class='text-xs text-gray-400 mb-3'>Pack of 25</p><div class='mt-auto flex justify-between items-center'><span class='text-cyan-400 font-bold'>Rs.180</span><button onclick=\"atc('BP Strip',180)\" class='bg-white/10 hover:bg-cyan-400 hover:text-black w-8 h-8 rounded-full transition flex items-center justify-center'><i class='fa-solid fa-plus'></i></button></div></div>";
  h += "<div class='gc p-4 flex flex-col'><div class='bg-white/5 h-28 rounded-lg mb-4 flex items-center justify-center text-gray-500'><i class='fa-solid fa-droplet text-4xl'></i></div><h3 class='font-bold'>Glucose Kit</h3><p class='text-xs text-gray-400 mb-3'>50 Strips</p><div class='mt-auto flex justify-between items-center'><span class='text-cyan-400 font-bold'>Rs.650</span><button onclick=\"atc('Glucose Kit',650)\" class='bg-white/10 hover:bg-cyan-400 hover:text-black w-8 h-8 rounded-full transition flex items-center justify-center'><i class='fa-solid fa-plus'></i></button></div></div>";
  h += "</div><div class='w-full lg:w-96 shrink-0'><div class='gc flex flex-col sticky top-4'>";
  h += "<div class='p-6 border-b border-white/10'><h2 class='font-bold text-xl flex items-center gap-2'><i class='fa-solid fa-cart-shopping text-cyan-400'></i> Your Order</h2></div>";
  h += "<div id='citems' class='p-6 space-y-4 max-h-80 overflow-y-auto'><p class='text-gray-500 text-center mt-10'>Cart is empty</p></div>";
  h += "<div class='p-6 border-t border-white/10'><div class='flex justify-between mb-2 text-gray-400 text-sm'><span>Subtotal</span><span id='csub'>Rs.0</span></div>";
  h += "<div class='flex justify-between mb-6 text-xl font-bold'><span>Total</span><span id='ctot'>Rs.0</span></div>";
  h += "<button onclick='chkout()' class='w-full bg-cyan-400 text-black font-bold py-4 rounded-xl hover:scale-105 transition'><i class='fa-solid fa-lock mr-2'></i>Proceed to Pay</button>";
  h += "</div></div></div></div></section>";

  h += "<section id='page-emergency' class='hidden space-y-6'>";
  h += "<header><h1 class='text-3xl font-bold text-red-400'>Emergency</h1><p class='text-gray-400'>Send WhatsApp alert</p></header>";
  h += "<div class='grid grid-cols-1 md:grid-cols-4 gap-6'>";
  h += "<div class='gc p-8 flex flex-col items-center gap-4 md:col-span-4'>";
  h += "<div class='bg-red-500/20 p-8 rounded-full pa cursor-pointer hover:scale-105 transition' onclick='trigSOS()'>";
  h += "<div class='bg-red-500 text-white w-32 h-32 rounded-full flex items-center justify-center text-4xl shadow-[0_0_40px_rgba(255,0,85,0.6)]'>SOS</div></div>";
  h += "<p class='text-gray-300 text-center'>Tap SOS to send WhatsApp alert with live BPM.</p>";
  h += "<p id='sosstatus' class='text-sm text-gray-400'></p>";
  h += "<p class='text-cyan-400 font-bold text-lg'>Current BPM: <span id='ebpm'>--</span></p></div>";
  h += "<a href='tel:108' class='gc p-6 flex flex-col items-center hover:bg-white/10 transition'><i class='fa-solid fa-truck-medical text-3xl text-red-400 mb-3'></i><span class='font-bold'>Ambulance</span><span class='text-sm text-gray-400'>Call 108</span></a>";
  h += "<a href='https://www.google.com/maps/search/hospitals+near+me' target='_blank' class='gc p-6 flex flex-col items-center hover:bg-white/10 transition'><i class='fa-solid fa-hospital text-3xl text-blue-400 mb-3'></i><span class='font-bold'>Find Hospital</span><span class='text-sm text-gray-400'>View on Map</span></a>";
  h += "<a href='https://www.google.com/maps/search/pharmacies+near+me' target='_blank' class='gc p-6 flex flex-col items-center hover:bg-white/10 transition'><i class='fa-solid fa-pills text-3xl text-purple-400 mb-3'></i><span class='font-bold'>Find Pharmacy</span><span class='text-sm text-gray-400'>View on Map</span></a>";
  h += "<div class='gc p-6 flex flex-col items-center hover:bg-white/10 transition cursor-pointer'><i class='fa-solid fa-user-doctor text-3xl text-green-400 mb-3'></i><span class='font-bold'>Dr. Sharma</span><span class='text-sm text-gray-400'>Personal Doctor</span></div>";
  h += "</div></section>";
  h += "</main>";

  // Payment Modal
  h += "<div id='paymodal' class='fixed inset-0 z-[999] bg-black/70 backdrop-blur-sm hidden flex items-center justify-center p-4'>";
  h += "<div class='gc w-full max-w-md p-8 relative'>";
  h += "<button onclick='closePayModal()' class='absolute top-4 right-4 text-gray-400 hover:text-white text-2xl'><i class='fa-solid fa-xmark'></i></button>";
  h += "<h2 class='text-2xl font-bold mb-1'>Complete Payment</h2>";
  h += "<p class='text-gray-400 text-sm mb-6'>Total: <span id='pay-total' class='text-cyan-400 font-bold text-lg'>Rs.0</span></p>";

  // Tabs
  h += "<div class='flex gap-2 mb-6'>";
  h += "<button id='tab-upi' onclick='switchTab(\"upi\")' class='flex-1 py-2 rounded-xl bg-cyan-400 text-black font-bold text-sm transition'>UPI</button>";
  h += "<button id='tab-card' onclick='switchTab(\"card\")' class='flex-1 py-2 rounded-xl bg-white/10 text-gray-300 font-bold text-sm transition'>Card</button>";
  h += "</div>";

  // UPI Tab
  h += "<div id='tab-upi-content'>";
  h += "<div class='flex flex-col items-center gap-4'>";
  h += "<div class='bg-white p-4 rounded-2xl'>";
  h += "<img src='https://api.qrserver.com/v1/create-qr-code/?size=160x160&data=upi://pay?pa=pulsecare@upi&pn=PulseCare&am=' id='qr-img' alt='QR Code' class='w-40 h-40'/>";
  h += "</div>";
  h += "<p class='text-xs text-gray-400'>Scan with any UPI app</p>";
  h += "<div class='w-full bg-white/5 border border-white/10 rounded-xl px-4 py-3 flex justify-between items-center'>";
  h += "<span class='text-sm text-gray-400'>UPI ID</span>";
  h += "<span class='text-sm font-bold text-cyan-400'>pulsecare@upi</span>";
  h += "<button onclick='copyUPI()' class='ml-3 text-gray-400 hover:text-cyan-400'><i class='fa-solid fa-copy'></i></button>";
  h += "</div>";
  h += "<button onclick='confirmPay()' class='w-full bg-cyan-400 text-black font-bold py-3 rounded-xl hover:scale-105 transition mt-2'>I have Paid</button>";
  h += "</div></div>";

  // Card Tab
  h += "<div id='tab-card-content' class='hidden space-y-4'>";
  h += "<div><label class='text-xs text-gray-400 mb-1 block'>Card Number</label>";
  h += "<input id='cnum' maxlength='19' placeholder='1234 5678 9012 3456' oninput='fmtCard(this)' class='w-full bg-white/5 border border-white/10 rounded-xl px-4 py-3 focus:outline-none focus:border-cyan-400 text-white placeholder-gray-600'/></div>";
  h += "<div class='flex gap-3'>";
  h += "<div class='flex-1'><label class='text-xs text-gray-400 mb-1 block'>Expiry</label>";
  h += "<input id='cexp' maxlength='5' placeholder='MM/YY' oninput='fmtExp(this)' class='w-full bg-white/5 border border-white/10 rounded-xl px-4 py-3 focus:outline-none focus:border-cyan-400 text-white placeholder-gray-600'/></div>";
  h += "<div class='flex-1'><label class='text-xs text-gray-400 mb-1 block'>CVV</label>";
  h += "<input id='ccvv' maxlength='3' placeholder='123' type='password' class='w-full bg-white/5 border border-white/10 rounded-xl px-4 py-3 focus:outline-none focus:border-cyan-400 text-white placeholder-gray-600'/></div></div>";
  h += "<div><label class='text-xs text-gray-400 mb-1 block'>Name on Card</label>";
  h += "<input id='cname' placeholder='Your Name' class='w-full bg-white/5 border border-white/10 rounded-xl px-4 py-3 focus:outline-none focus:border-cyan-400 text-white placeholder-gray-600'/></div>";
  h += "<button onclick='payCard()' class='w-full bg-cyan-400 text-black font-bold py-3 rounded-xl hover:scale-105 transition'><i class='fa-solid fa-lock mr-2'></i>Pay Now</button>";
  h += "</div>";
  h += "</div></div>";
  h += "</div>";

  h += "<nav class='fixed bottom-0 left-0 w-full gc border-t border-white/10 z-50 md:hidden flex justify-around items-center p-3 pb-5 backdrop-blur-xl bg-slate-900/90 rounded-t-2xl'>";
  h += "<button onclick=\"sp('dashboard')\" id='mnav-dashboard' class='mn active text-cyan-400 flex flex-col items-center gap-1 w-14'><i class='fa-solid fa-chart-line text-xl'></i><span class='text-[10px]'>Home</span></button>";
  h += "<button onclick=\"sp('history')\" id='mnav-history' class='mn text-gray-400 flex flex-col items-center gap-1 w-14'><i class='fa-solid fa-clock-rotate-left text-xl'></i><span class='text-[10px]'>History</span></button>";
  h += "<button onclick=\"sp('emergency')\" class='bg-red-500 text-white w-14 h-14 rounded-full flex items-center justify-center -mt-10 border-4 border-[#0f172a] shadow-lg hover:scale-110 transition'><i class='fa-solid fa-bell text-2xl'></i></button>";
  h += "<button onclick=\"sp('pharmacy')\" id='mnav-pharmacy' class='mn text-gray-400 flex flex-col items-center gap-1 w-14'><i class='fa-solid fa-pills text-xl'></i><span class='text-[10px]'>Meds</span></button>";
  h += "<button onclick=\"sp('profile')\" id='mnav-profile' class='mn text-gray-400 flex flex-col items-center gap-1 w-14'><i class='fa-solid fa-user text-xl'></i><span class='text-[10px]'>Patient</span></button>";
  h += "</nav>";

  h += "<script>";
  h += "function sp(id){";
  h += "document.querySelectorAll('main > section').forEach(function(s){s.classList.add('hidden');});";
  h += "document.getElementById('page-'+id).classList.remove('hidden');";
  h += "document.querySelectorAll('.ni').forEach(function(n){n.classList.remove('active');});";
  h += "var d=document.getElementById('nav-'+id); if(d) d.classList.add('active');";
  h += "document.querySelectorAll('.mn').forEach(function(n){n.classList.remove('active','text-cyan-400');n.classList.add('text-gray-400');});";
  h += "var m=document.getElementById('mnav-'+id);";
  h += "if(m){m.classList.add('active','text-cyan-400');m.classList.remove('text-gray-400');}";
  h += "}";
  h += "var ctx=document.getElementById('hrc').getContext('2d');";
  h += "var MP=40;";
  h += "var chart=new Chart(ctx,{type:'line',data:{labels:Array(MP).fill(''),datasets:[{label:'BPM',data:Array(MP).fill(null),borderColor:'#00f2ea',borderWidth:2,pointRadius:0,tension:0.1,fill:true,backgroundColor:'rgba(0,242,234,0.05)'}]},options:{responsive:true,maintainAspectRatio:false,animation:false,plugins:{legend:{display:false}},scales:{x:{display:false},y:{min:40,max:160,grid:{color:'rgba(255,255,255,0.05)'},ticks:{color:'#94a3b8'}}}}});";
  h += "var bd=document.getElementById('bpm-display');";
  h += "var bst=document.getElementById('bstt');";
  h += "var bsd=document.getElementById('bstd');";
  h += "var bc=document.getElementById('bpm-card');";
  h += "var smin=document.getElementById('smin');";
  h += "var smax=document.getElementById('smax');";
  h += "var savg=document.getElementById('savg');";
  h += "var pbpm=document.getElementById('pbpm');";
  h += "var ebpm=document.getElementById('ebpm');";
  h += "var htbl=document.getElementById('htbl');";
  h += "var hemp=document.getElementById('hemp');";
  h += "var toast=document.getElementById('toast');";
  h += "var tmsg=document.getElementById('toast-msg');";
  h += "var tt=null; var lab=0;";
  h += "function showT(msg,col){toast.className='toast '+col+' text-white shadow-lg';tmsg.innerText=msg;toast.style.display='block';clearTimeout(tt);tt=setTimeout(function(){toast.style.display='none';},5000);}";
  h += "function fetchBPM(){";
  h += "fetch('/bpm').then(function(r){return r.json();}).then(function(data){";
  h += "var bpm=data.bpm; var fi=data.finger;";
  h += "pbpm.innerText=fi?bpm:'--'; ebpm.innerText=fi?bpm:'--';";
  h += "if(data.min>0) smin.innerText=data.min;";
  h += "if(data.max>0) smax.innerText=data.max;";
  h += "if(data.avg>0) savg.innerText=data.avg;";
  h += "if(!fi||bpm===0){bd.innerText='--';bst.innerText='No finger detected';bst.className='text-gray-400 font-medium';bsd.className='w-2 h-2 rounded-full bg-gray-400';bc.classList.remove('border-red-500');chart.data.datasets[0].borderColor='#00f2ea';}";
  h += "else{bd.innerText=bpm;chart.data.labels.push('');chart.data.datasets[0].data.push(bpm);";
  h += "if(chart.data.datasets[0].data.length>MP){chart.data.labels.shift();chart.data.datasets[0].data.shift();}";
  h += "chart.update('none');";
  h += "if(bpm>100||bpm<60){bst.innerText='Abnormal Rhythm';bst.className='text-red-400 font-bold';bsd.className='w-2 h-2 rounded-full bg-red-400';bc.classList.add('border-red-500');chart.data.datasets[0].borderColor='#ff0055';if(bpm!==lab){showT('Abnormal BPM: '+bpm+' - WhatsApp sent!','bg-red-600');lab=bpm;}}";
  h += "else{bst.innerText='Normal Rhythm';bst.className='text-green-400 font-medium';bsd.className='w-2 h-2 rounded-full bg-green-400';bc.classList.remove('border-red-500');chart.data.datasets[0].borderColor='#00f2ea';lab=0;}}";
  h += "if(data.history&&data.history.length>0){hemp.style.display='none';htbl.innerHTML='';";
  h += "var rows=data.history.slice().reverse();";
  h += "rows.forEach(function(r,i){var s=r.bpm>100?'<span class=\"text-red-400\">High</span>':r.bpm<60?'<span class=\"text-yellow-400\">Low</span>':'<span class=\"text-green-400\">Normal</span>';";
  h += "htbl.innerHTML+='<tr class=\"border-b border-white/5 hover:bg-white/5\"><td class=\"py-3 text-gray-500\">'+(rows.length-i)+'</td><td class=\"py-3 font-bold\">'+r.bpm+'</td><td class=\"py-3\">'+s+'</td><td class=\"py-3 text-gray-400\">'+r.ts+'s</td></tr>';});}";
  h += "}).catch(function(){bst.innerText='Sensor offline';bst.className='text-gray-500';});}";
  h += "setInterval(fetchBPM,1000); fetchBPM();";
  h += "function updateClock(){var now=new Date();";
  h += "var h2=String(now.getHours()).padStart(2,'0');";
  h += "var m2=String(now.getMinutes()).padStart(2,'0');";
  h += "var s2=String(now.getSeconds()).padStart(2,'0');";
  h += "var days=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];";
  h += "var months=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];";
  h += "document.getElementById('clock').innerText=h2+':'+m2+':'+s2;";
  h += "document.getElementById('clockdate').innerText=days[now.getDay()]+', '+now.getDate()+' '+months[now.getMonth()]+' '+now.getFullYear();}";
  h += "setInterval(updateClock,1000); updateClock();";
  h += "function trigSOS(){var bpm=bd.innerText;var ss=document.getElementById('sosstatus');ss.innerText='Sending WhatsApp...';ss.className='text-sm text-yellow-400';";
  h += "fetch('/sos?bpm='+bpm).then(function(r){if(r.ok){ss.innerText='WhatsApp sent!';ss.className='text-sm text-green-400 font-bold';showT('SOS sent! BPM: '+bpm,'bg-green-600');}else{ss.innerText='Failed. Try again.';ss.className='text-sm text-red-400';}}).catch(function(){ss.innerText='Error. Check WiFi.';ss.className='text-sm text-red-400';});}";
  h += "var cart=[];";
  h += "function atc(name,price){cart.push({name:name,price:price});rc();}";
  h += "function rfc(i){cart.splice(i,1);rc();}";
  h += "function rc(){var el=document.getElementById('citems');var tot=document.getElementById('ctot');var sub=document.getElementById('csub');";
  h += "if(!cart.length){el.innerHTML='<p class=\"text-gray-500 text-center mt-10\">Cart is empty</p>';tot.innerText=sub.innerText='Rs.0';return;}";
  h += "var html='',total=0;";
  h += "cart.forEach(function(item,i){total+=item.price;html+='<div class=\"flex justify-between items-center bg-white/5 p-3 rounded-lg\"><div><p class=\"font-bold text-sm\">'+item.name+'</p></div><div class=\"flex items-center gap-3\"><span class=\"text-sm font-bold\">Rs.'+item.price+'</span><button onclick=\"rfc('+i+')\" class=\"text-gray-500 hover:text-red-400\"><i class=\"fa-solid fa-trash-can\"></i></button></div></div>';});";
  h += "el.innerHTML=html;tot.innerText=sub.innerText='Rs.'+total;}";
  h += "function chkout(){if(!cart.length){alert('Cart is empty!');return;}";
  h += "var total=0; cart.forEach(function(i){total+=i.price;});";
  h += "document.getElementById('pay-total').innerText='Rs.'+total;";
  h += "var qr=document.getElementById('qr-img');";
  h += "qr.src='https://api.qrserver.com/v1/create-qr-code/?size=160x160&data=upi://pay?pa=pulsecare@upi%26pn=PulseCare%26am='+total+'%26cu=INR';";
  h += "document.getElementById('paymodal').classList.remove('hidden');";
  h += "document.getElementById('paymodal').classList.add('flex');}";
  h += "function closePayModal(){document.getElementById('paymodal').classList.add('hidden');document.getElementById('paymodal').classList.remove('flex');}";
  h += "function switchTab(t){";
  h += "document.getElementById('tab-upi-content').classList.add('hidden');";
  h += "document.getElementById('tab-card-content').classList.add('hidden');";
  h += "document.getElementById('tab-upi').className='flex-1 py-2 rounded-xl bg-white/10 text-gray-300 font-bold text-sm transition';";
  h += "document.getElementById('tab-card').className='flex-1 py-2 rounded-xl bg-white/10 text-gray-300 font-bold text-sm transition';";
  h += "document.getElementById('tab-'+t+'-content').classList.remove('hidden');";
  h += "document.getElementById('tab-'+t).className='flex-1 py-2 rounded-xl bg-cyan-400 text-black font-bold text-sm transition';}";
  h += "function copyUPI(){navigator.clipboard.writeText('pulsecare@upi');showT('UPI ID copied!','bg-green-600');}";
  h += "function confirmPay(){closePayModal();showT('Payment confirmed! Order placed.','bg-green-600');cart=[];rc();}";
  h += "function fmtCard(el){var v=el.value.replace(/\\D/g,'').substring(0,16);el.value=v.replace(/(\\d{4})/g,'$1 ').trim();}";
  h += "function fmtExp(el){var v=el.value.replace(/\\D/g,'');if(v.length>=2)v=v.substring(0,2)+'/'+v.substring(2,4);el.value=v;}";
  h += "function payCard(){var n=document.getElementById('cnum').value.replace(/\\s/g,'');var e=document.getElementById('cexp').value;var c=document.getElementById('ccvv').value;";
  h += "if(n.length<16||e.length<5||c.length<3){alert('Please fill all card details correctly!');return;}";
  h += "closePayModal();showT('Payment successful! Order placed.','bg-green-600');cart=[];rc();}";
  h += "function savePro(){document.getElementById('sname').innerText=document.getElementById('pname').value;showT('Profile saved!','bg-green-600');}";
  h += "function dlCSV(){";
  h += "fetch('/bpm').then(function(r){return r.json();}).then(function(data){";
  h += "if(!data.history||data.history.length===0){alert('No data to download yet!');return;}";
  h += "var csv='#,BPM,Status,Time(s since boot)\\n';";
  h += "var rows=data.history.slice().reverse();";
  h += "rows.forEach(function(r,i){";
  h += "var s=r.bpm>100?'High':r.bpm<60?'Low':'Normal';";
  h += "csv+=(i+1)+','+r.bpm+','+s+','+r.ts+'\\n';});";
  h += "var blob=new Blob([csv],{type:'text/csv'});";
  h += "var url=URL.createObjectURL(blob);";
  h += "var a=document.createElement('a');";
  h += "a.href=url; a.download='BPM_History_'+new Date().toISOString().slice(0,10)+'.csv';";
  h += "a.click(); URL.revokeObjectURL(url);";
  h += "showT('CSV downloaded!','bg-green-600');";
  h += "}).catch(function(){alert('Failed to fetch data.');});}";
  h += "function addRec(){var t=prompt('Condition:');if(!t)return;var d=prompt('Date:')||'Recent';var n=prompt('Notes:')||'';";
  h += "var cols=['border-green-500','border-blue-500','border-yellow-500','border-purple-500'];";
  h += "var li=document.createElement('li');";
  h += "li.className='bg-white/5 p-4 rounded-lg border-l-4 '+cols[Math.floor(Math.random()*cols.length)];";
  h += "li.innerHTML='<h4 class=\"font-bold text-sm\">'+t+'</h4><p class=\"text-xs text-gray-400 mt-1\">'+d+'</p><p class=\"text-sm text-gray-300 mt-2\">'+n+'</p>';";
  h += "document.getElementById('mrecs').prepend(li);}";
  h += "</script></body></html>";

  server.send(200, "text/html", h);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());
  server.on("/",    handleRoot);
  server.on("/bpm", handleBPM);
  server.on("/sos", handleSOS);
  server.begin();
  Serial.println("Server ready.");
}

void loop() {
  server.handleClient();
  int signal = analogRead(PULSE_PIN);

  if (signal < FINGER_THRESHOLD) {
    fingerPresent = false; bpm = 0;
    abnormalAlertSent = false; abnormalSince = 0;
    return;
  }

  fingerPresent = true;
  static int lastSignal = 0;
  unsigned long now = millis();

  if (signal > BEAT_THRESHOLD && lastSignal <= BEAT_THRESHOLD) {
    if (lastBeatTime > 0) {
      unsigned long interval = now - lastBeatTime;
      if (interval > 300 && interval < 2000) {
        int instant = 60000 / interval;
        bpmSamples[sampleIndex] = instant;
        sampleIndex = (sampleIndex + 1) % SAMPLE_SIZE;
        int sum = 0;
        for (int i = 0; i < SAMPLE_SIZE; i++) sum += bpmSamples[i];
        bpm = sum / SAMPLE_SIZE;
        addHistory(bpm);

        if (bpm > 100 || bpm < 60) {
          if (abnormalSince == 0) abnormalSince = now;
          if (!abnormalAlertSent && (now - abnormalSince > ABNORMAL_ALERT_DELAY)) {
            String msg = "ABNORMAL+HEART+RATE!+BPM:+" + String(bpm) + "+Please+check+patient+immediately!";
            sendWhatsApp(msg);
            abnormalAlertSent = true;
            Serial.println("Auto alert sent! BPM: " + String(bpm));
          }
        } else {
          abnormalSince = 0;
          abnormalAlertSent = false;
        }
      }
    }
    lastBeatTime = now;
  }
  lastSignal = signal;
  delay(10);
}
