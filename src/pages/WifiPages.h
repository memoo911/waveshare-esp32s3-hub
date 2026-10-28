#pragma once

// WiFi: saved networks, setup from a phone (QR codes) and the location page.

#include "qrcodegen.h"
#include "ui/Widgets.h"

namespace ui {

// ---------------------------------------------------------------------------
// QR code
// ---------------------------------------------------------------------------
// Draw `text` as a QR code on a white rounded card centered at (cx, cy),
// at most `maxSize` px wide. Returns false if the text didn't fit.
inline bool drawQr(Canvas &c, const char *text, int cx, int cy, int maxSize) {
  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_LOW, 1, 10,
                            qrcodegen_Mask_AUTO, true)) {
    return false;
  }
  const int n = qrcodegen_getSize(qr);
  const int quiet = 3;
  const int module = maxSize / (n + quiet * 2);
  const int size = module * (n + quiet * 2);
  const int x0 = cx - size / 2, y0 = cy - size / 2;
  c.roundRect(x0, y0, size, size, module * 2, rgb(0xFFFFFF));
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      if (qrcodegen_getModule(qr, x, y)) {
        c.fillRect(x0 + (x + quiet) * module, y0 + (y + quiet) * module, module, module,
                   rgb(0x000000));
      }
    }
  }
  return true;
}

// Text for a "join this WiFi" QR code (understood by iOS and Android cameras).
inline void wifiQrText(char *out, size_t n, const char *ssid, const char *password) {
  snprintf(out, n, "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);
}


// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
constexpr int kNetRowsTop = 164;
constexpr int kNetRowH = 42;
constexpr int kNetRowGap = 4;
constexpr int kNetRowsMax = 5;
constexpr Rect kAddNetworkButton{44, 394, 280, 44};

inline Rect netRowRect(int i) {
  return Rect{14, kNetRowsTop + i * (kNetRowH + kNetRowGap), 340, kNetRowH};
}
// The "×" delete button at the right end of a row.
inline Rect netDeleteRect(int i) {
  const Rect r = netRowRect(i);
  return Rect{r.x + r.w - 56, r.y, 56, r.h};
}

inline void drawWifiStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);
  drawTitleBar(c, s, i18n::tr(L, Str::Wifi));

  // Status: signal bars, network and address.
  c.roundRect(14, 70, 340, 82, 22, rgb(0x14171D));
  char buf[64];
  if (s.wifiConnected && d.ssid[0]) {
    const int bars = d.rssi > -55 ? 4 : d.rssi > -65 ? 3 : d.rssi > -75 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
      const float h = 8 + i * 6;
      c.roundRect(34 + i * 11, 124 - h, 7, h, 2.5f, i < bars ? kAccent : kFaint);
    }
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::ConnectedTo), d.ssid);
    const bool fits = c.textWidth(kFontBody, buf) <= 250;
    c.text(fits ? kFontBody : kFontSmall, 92, 104, buf, kText);
    snprintf(buf, sizeof(buf), "%d dBm · %s", d.rssi, d.ip);
    c.text(kFontSmall, 92, 130, buf, kMuted);
  } else {
    c.text(kFontBody, W / 2, 118, i18n::tr(L, Str::NotConnected), kText, Align::Center);
  }

  // Saved networks with a delete button (tap twice to delete).
  if (d.netCount == 0) {
    c.text(kFontSmall, W / 2, 196, i18n::tr(L, Str::NoSavedNetworks), kMuted, Align::Center);
  }
  for (int i = 0; i < d.netCount && i < kNetRowsMax; i++) {
    const Rect r = netRowRect(i);
    const bool confirm = d.pendingDelete == i;
    c.roundRect(r.x, r.y, r.w, r.h, 14, confirm ? rgb(0x3A1C1C) : rgb(0x14171D));
    const bool current = s.wifiConnected && strcmp(d.nets[i].ssid, d.ssid) == 0;
    if (current) c.fillCircle(r.x + 18, r.y + r.h / 2, 4, kAccent);
    c.text(kFontBody, r.x + 32, r.y + 28, d.nets[i].ssid, kText);
    if (d.nets[i].fromCode) {
      c.text(kFontSmall, r.x + r.w - 18, r.y + 27, i18n::tr(L, Str::FromCode), kFaint,
             Align::Right);
    } else if (confirm) {
      c.text(kFontSmall, r.x + r.w - 48, r.y + 27, i18n::tr(L, Str::DeleteConfirm),
             screen::kWarn, Align::Right);
      const float cx = r.x + r.w - 26, cy = r.y + r.h / 2.0f;
      c.capsule(cx - 6, cy - 6, cx + 6, cy + 6, 1.6f, screen::kWarn);
      c.capsule(cx - 6, cy + 6, cx + 6, cy - 6, 1.6f, screen::kWarn);
    } else {
      const float cx = r.x + r.w - 26, cy = r.y + r.h / 2.0f;
      c.capsule(cx - 6, cy - 6, cx + 6, cy + 6, 1.6f, kMuted);
      c.capsule(cx - 6, cy + 6, cx + 6, cy - 6, 1.6f, kMuted);
    }
  }

  const Rect &b = kAddNetworkButton;
  c.roundRect(b.x, b.y, b.w, b.h, b.h / 2, rgb(0x1E3A33));
  c.text(kFontBody, b.x + b.w / 2, b.y + 30, i18n::tr(L, Str::AddNetwork), kAccent,
         Align::Center);
}

// Index of the saved network whose "×" was tapped, or -1.
inline int wifiDeleteAt(const UiState &st, int x, int y) {
  if (st.page != Page::Wifi) return -1;
  for (int i = 0; i < st.info.netCount && i < kNetRowsMax; i++) {
    if (!st.info.nets[i].fromCode && inside(netDeleteRect(i), x, y, kNetRowGap / 2)) return i;
  }
  return -1;
}

// QR code for the board's own access point, plus the same details as text.
inline void drawWifiSetupStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);
  drawTitleBar(c, s, i18n::tr(L, Str::Wifi));
  char buf[96];
  if (d.setupState == SetupState::Starting) {
    screen::fitText(c, W / 2, 240, i18n::tr(L, Str::WifiStarting), kMuted);
    return;
  }
  if (d.setupState == SetupState::Saved) {
    snprintf(buf, sizeof(buf), i18n::tr(L, d.setupOverLan ? Str::SavedLan : Str::WifiSaved),
             d.savedSsid);
    screen::fitText(c, W / 2, 240, buf, kText);
    return;
  }
  if (d.setupOverLan) {
    // The board is online: the phone opens its address on the same WiFi.
    snprintf(buf, sizeof(buf), "http://%s/", d.ip);
    drawQr(c, buf, W / 2, 178, 200);
    c.text(kFontSmall, 34, 310, i18n::tr(L, Str::WifiStep1), kText);
    c.text(kFontSmall, 34, 334, i18n::tr(L, Str::SetupLanStep2), kText);
    c.roundRect(14, 352, 340, 82, 20, rgb(0x14171D));
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::ConnectedTo), d.ssid);
    screen::fitText(c, W / 2, 386, buf, kText);
    c.text(kFontSmall, W / 2, 416, i18n::tr(L, Str::SetupLanNote), kMuted, Align::Center);
    return;
  }
  if (d.apClients > 0) {
    // Step 2: the phone is on the board's network; this QR opens the form,
    // for phones that don't pop up the sign-in page by themselves.
    drawQr(c, "http://192.168.4.1/", W / 2, 178, 200);
    c.fillCircle(46, 303, 9, kAccent);
    c.capsule(42, 303, 45, 307, 1.6f, kBackground);
    c.capsule(45, 307, 51, 299, 1.6f, kBackground);
    c.text(kFontBody, 64, 310, i18n::tr(L, Str::PhoneConnected), kAccent);
    c.text(kFontSmall, 34, 340, i18n::tr(L, Str::ScanAgain), kText);
    c.roundRect(14, 360, 340, 74, 20, rgb(0x14171D));
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::OrType), "192.168.4.1");
    c.text(kFontSmall, W / 2, 404, buf, kMuted, Align::Center);
    return;
  }
  wifiQrText(buf, sizeof(buf), d.apSsid, d.apPassword);
  drawQr(c, buf, W / 2, 178, 200);
  c.text(kFontSmall, 34, 310, i18n::tr(L, Str::WifiStep1), kText);
  c.text(kFontSmall, 34, 334, i18n::tr(L, Str::WifiStep2), kText);
  c.roundRect(14, 352, 340, 82, 20, rgb(0x14171D));
  c.text(kFontSmall, 34, 384, i18n::tr(L, Str::NetworkLabel), kMuted);
  c.text(kFontBody, 334, 385, d.apSsid, kText, Align::Right);
  c.text(kFontSmall, 34, 418, i18n::tr(L, Str::PasswordLabel), kMuted);
  c.text(kFontBody, 334, 419, d.apPassword, kText, Align::Right);
}


// ---------------------------------------------------------------------------
// Location
// ---------------------------------------------------------------------------
inline void drawLocationStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);
  drawTitleBar(c, s, i18n::tr(L, Str::Location));
  char buf[64];
  if (!s.wifiConnected || !d.ip[0]) {
    screen::fitText(c, W / 2, 240, i18n::tr(L, Str::LocationNeedsWifi), kMuted);
    return;
  }
  snprintf(buf, sizeof(buf), "http://%s/", d.ip);
  drawQr(c, buf, W / 2, 178, 200);
  c.text(kFontSmall, 34, 310, i18n::tr(L, Str::LocationStep1), kText);
  c.text(kFontSmall, 34, 334, i18n::tr(L, Str::LocationStep2), kText);
  c.roundRect(14, 352, 340, 82, 20, rgb(0x14171D));
  snprintf(buf, sizeof(buf), i18n::tr(L, Str::Current), s.city);
  screen::fitText(c, W / 2, 386, buf, kText);
  c.text(kFontSmall, W / 2, 416, i18n::tr(L, Str::LocationSameWifi), kMuted, Align::Center);
}

} // namespace ui
