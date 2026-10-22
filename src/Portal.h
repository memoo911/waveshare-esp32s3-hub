#pragma once

// Web pages the board serves to a phone:
// - WiFi setup (captive portal on the board's own access point)
// - Location picker (on the home network, while the Location page is open)
//
// The location page runs in the phone's browser and searches places with
// the Open-Meteo geocoding API directly (the phone has internet), then posts
// the chosen place to the board. Both pages also send the phone's UTC offset
// as a fallback for the clock's time zone.

namespace portal {

// Shared look: dark, large touch targets, works on any phone browser.
static const char kStyle[] = R"CSS(
<style>
*{box-sizing:border-box}body{margin:0;font-family:-apple-system,system-ui,Segoe UI,Roboto,sans-serif;
background:#0b0d11;color:#f3f5f8;padding:24px 18px;max-width:520px;margin:auto}
h1{font-size:24px;margin:0 0 6px}p.sub{color:#8c94a1;margin:0 0 22px}
label{display:block;color:#8c94a1;font-size:14px;margin:16px 0 6px}
input,select{width:100%;font-size:17px;padding:14px;border-radius:14px;border:1px solid #2a2f38;
background:#14171d;color:#f3f5f8}
button{width:100%;font-size:17px;font-weight:600;padding:15px;border:0;border-radius:999px;
background:#6fd3b1;color:#08261d;margin-top:22px}
.item{padding:14px 16px;border-radius:14px;background:#14171d;margin:8px 0;cursor:pointer}
.item small{display:block;color:#8c94a1;margin-top:3px}.ok{color:#6fd3b1}.err{color:#ff8a65}
.chip{display:inline-block;padding:9px 13px;margin:8px 8px 0 0;border-radius:999px;
background:#1b1f27;color:#d7dce3;font-size:15px;cursor:pointer}
</style>)CSS";

// --- WiFi setup ------------------------------------------------------------
// %LANG% texts are filled in by the board; %OPTIONS% is the network list.
static const char kWifiPage[] = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>%TITLE%</title>%STYLE%</head><body>
<h1>%TITLE%</h1><p class="sub">%SUB%</p>
<form method="post" action="/save">
<label>%NETWORK%</label>
<input name="ssid" id="ssid" placeholder="%HINT%" autocomplete="off" autocapitalize="none" required>
<div id="near"><label>%NEARBY%</label>%CHIPS%</div>
<label>%PASSWORD%</label>
<input name="pass" type="password" autocomplete="off">
<input name="tz" id="tz" type="hidden">
<button type="submit">%SAVE%</button>
</form>
<script>tz.value=-new Date().getTimezoneOffset()*60;
near.onclick=e=>{if(e.target.classList.contains("chip"))ssid.value=e.target.textContent;};
if(!near.querySelector(".chip"))near.style.display="none";</script>
</body></html>)HTML";

static const char kWifiSaved[] = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
%STYLE%</head><body><h1 class="ok">%DONE%</h1><p class="sub">%NEXT%</p></body></html>)HTML";

// --- Location --------------------------------------------------------------
static const char kLocationPage[] = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>%TITLE%</title>%STYLE%</head><body>
<h1>%TITLE%</h1><p class="sub">%NOW%: <b id="cur">%CITY%</b></p>
<form id="f"><input id="q" placeholder="%PLACEHOLDER%" autocomplete="off">
<button type="submit">%SEARCH%</button></form>
<div id="res"></div><p id="msg"></p>
<script>
const L="%LANG%";
f.onsubmit=async e=>{e.preventDefault();res.innerHTML="";msg.textContent="…";
 try{const r=await fetch("https://geocoding-api.open-meteo.com/v1/search?count=8&format=json&language="+L+"&name="+encodeURIComponent(q.value));
  const j=await r.json();msg.textContent=(j.results&&j.results.length)?"":"%NONE%";
  (j.results||[]).forEach(p=>{const d=document.createElement("div");d.className="item";
   const sub=[p.admin1,p.country].filter(Boolean).join(", ");
   d.innerHTML="";d.append(p.name);const s=document.createElement("small");s.textContent=sub;d.append(s);
   d.onclick=()=>save(p);res.append(d);});
 }catch(err){msg.className="err";msg.textContent="%OFFLINE%";}};
async function save(p){const b=new URLSearchParams({name:p.name,lat:p.latitude,lon:p.longitude,
 tz:-new Date().getTimezoneOffset()*60});
 const r=await fetch("/api/location",{method:"POST",body:b});
 if(r.ok){cur.textContent=p.name;res.innerHTML="";msg.className="ok";msg.textContent="%SAVED%";}
 else{msg.className="err";msg.textContent="%FAILED%";}}
</script></body></html>)HTML";

struct Texts {
  const char *wifiTitle, *wifiSub, *network, *hint, *nearby, *password, *save, *done, *next,
      *nextLan;
  const char *locTitle, *now, *placeholder, *search, *none, *offline, *saved, *failed;
  const char *lang;
};

inline const Texts &texts(bool hr) {
  static const Texts kEn = {
      "Add WiFi network",
      "Type the network's name and password. It can also be a network that is off "
      "right now, like your phone's hotspot.",
      "Network name", "e.g. your phone's hotspot", "Nearby (tap to fill in)", "Password",
      "Save", "Saved", "The board is connecting now. You can close this page.",
      "The board will connect to it whenever it's in range. You can close this page.",
      "Location", "Now", "City or village, e.g. Zagreb", "Search", "Nothing found",
      "No internet on this phone?", "Saved, the board is updating", "Couldn't save", "en"};
  static const Texts kHr = {
      "Dodaj WiFi mrežu",
      "Upiši naziv mreže i lozinku. Može i mreža koja sad nije upaljena, npr. hotspot "
      "tvog mobitela.",
      "Naziv mreže", "npr. hotspot mobitela", "U blizini (dodirni za upis)", "Lozinka",
      "Spremi", "Spremljeno", "Ploča se sada spaja. Možeš zatvoriti ovu stranicu.",
      "Ploča će se spojiti na nju kad god bude u dometu. Možeš zatvoriti ovu stranicu.",
      "Mjesto", "Sada", "Grad ili mjesto, npr. Zagreb", "Traži", "Ništa nije pronađeno",
      "Mobitel nema interneta?", "Spremljeno, ploča se osvježava", "Spremanje nije uspjelo",
      "hr"};
  return hr ? kHr : kEn;
}

// Escape text for HTML.
inline String escape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    const char c = in[i];
    switch (c) {
    case '&': out += "&amp;"; break;
    case '<': out += "&lt;"; break;
    case '>': out += "&gt;"; break;
    case '"': out += "&quot;"; break;
    case '\'': out += "&#39;"; break;
    default: out += c;
    }
  }
  return out;
}

} // namespace portal
