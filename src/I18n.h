#pragma once

// UI text in English (default) and Croatian. Every visible string goes
// through tr() so the language can be switched in Settings.

#include <stdint.h>

enum class Lang : uint8_t { En = 0, Hr = 1 };

enum class Str : uint8_t {
  FeelsLike,
  Sunset,
  Sunrise,
  MinMax,
  Humidity,
  Wind,
  NoWifi,
  Outdated,
  Refreshing,
  LoadingForecast,
  ForecastUnavailable,
  ForecastNotYet,
  Tomorrow,
  ConnectingWifi,
  ConnectingTo,      // "%s"
  CantFind,          // "%s"
  WrongPassword,     // "%s"
  WifiUnavailable,   // "%s"
  SetUpWifi,
  Weather,
  Settings,
  Wifi,
  Location,
  Brightness,
  Sound,
  Language,
  NotConnected,
  SoundSoon,
  DragHint,
  NightHint,
  ConnectedTo,       // "%s"
  SavedNetworks,     // "%d"
  AddNetwork,
  WifiStep1,
  WifiStep2,
  NetworkLabel,
  PasswordLabel,
  WifiStarting,
  WifiSaved,         // "%s"
  LocationStep1,
  LocationStep2,
  LocationSameWifi,
  LocationNeedsWifi,
  Current,           // "%s"
  LanguageName,
  SetupLanStep2,
  SetupLanNote,
  DeleteConfirm,
  FromCode,
  SavedLan,          // "%s"
  NoSavedNetworks,
  PhoneConnected,
  ScanAgain,
  OrType,            // "%s"
  Apps,
  HourlyChime,
  ChimeHint,
  ScreenOffAfter,
  Never,
  WakeHint,
  TurnOff,
  Cancel,
  TurnOffQuestion,
  TurnOnHint,
  NightDimming,
  TimerTile,
  KitchenTimer,
  Start,
  Pause,
  Resume,
  Reset,
  Stop,
  TimesUp,
  AppsHint,
  LevelTile,
  SpiritLevel,
  SetZero,
  LevelOk,
  NoSensor,
  AvatarApp,
  AvWave,
  AvListen,
  AvDance,
  AvRest,
  AvType,
  AvSleep,
  Count
};

namespace i18n {

inline const char *tr(Lang lang, Str id) {
  static const char *const kEn[] = {
      "Feels like %s",
      "Sunset %s",
      "Sunrise %s",
      "Low / High",
      "Humidity",
      "Wind",
      "· no WiFi",
      "· outdated",
      "Refreshing…",
      "Loading forecast…",
      "Forecast unavailable",
      "Forecast not loaded yet",
      "Tomorrow",
      "Connecting to WiFi…",
      "Connecting to %s…",
      "Can't find %s",
      "Wrong password for %s?",
      "WiFi unavailable (%s)",
      "Set up WiFi in Settings",
      "Weather",
      "Settings",
      "WiFi",
      "Location",
      "Display",
      "Sound",
      "Language",
      "Not connected",
      "More apps coming later",
      "Drag left or right",
      "Dimmer from 22:00 to 07:00",
      "Connected to %s",
      "Saved networks: %d",
      "Add network",
      "1. Scan with your phone camera",
      "2. Choose WiFi, enter password",
      "Network",
      "Password",
      "Starting setup…",
      "Saved, connecting to %s…",
      "1. Scan with your phone camera",
      "2. Type a place and tap it",
      "Phone must be on the same WiFi",
      "Connect to WiFi first",
      "Now: %s",
      "English",
      "2. Enter WiFi name and password",
      "Phone stays on this WiFi",
      "Delete?",
      "in code",
      "Saved: %s",
      "No saved networks",
      "Phone connected",
      "Now scan this to open the page",
      "or open %s in the browser",
      "Apps",
      "Hourly chime",
      "Silent at night (22:00 - 07:00)",
      "Screen off after",
      "Never",
      "Touch or pick up to wake",
      "Turn off",
      "Cancel",
      "Turn off the clock?",
      "Press PWR to turn it on again",
      "Night dimming",
      "Timer",
      "Kitchen timer",
      "Start",
      "Pause",
      "Resume",
      "Reset",
      "Stop",
      "Time's up!",
      "The chosen app is on the home screen",
      "Level",
      "Spirit level",
      "Set zero",
      "Level",
      "Motion sensor not found",
      "Avatar",
      "Waving",
      "Listening to music",
      "Dancing",
      "Resting",
      "Typing",
      "Sleeping",
  };
  static const char *const kHr[] = {
      "Osjeća se %s",
      "Zalazak %s",
      "Izlazak %s",
      "Min / Max",
      "Vlaga",
      "Vjetar",
      "· bez WiFi",
      "· staro",
      "Osvježavam…",
      "Učitavam prognozu…",
      "Prognoza nije dostupna",
      "Prognoza još nije stigla",
      "Sutra",
      "Spajam se na WiFi…",
      "Spajam se na %s…",
      "Ne vidim mrežu %s",
      "Kriva lozinka za %s?",
      "WiFi nije dostupan (%s)",
      "Postavi WiFi u Postavkama",
      "Vrijeme",
      "Postavke",
      "WiFi",
      "Mjesto",
      "Zaslon",
      "Zvuk",
      "Jezik",
      "Nije spojeno",
      "Još aplikacija stiže kasnije",
      "Povuci prstom lijevo ili desno",
      "Tamnije od 22:00 do 07:00",
      "Spojeno na %s",
      "Spremljene mreže: %d",
      "Dodaj mrežu",
      "1. Skeniraj kamerom mobitela",
      "2. Odaberi WiFi i upiši lozinku",
      "Mreža",
      "Lozinka",
      "Pokrećem postavljanje…",
      "Spremljeno, spajam se na %s…",
      "1. Skeniraj kamerom mobitela",
      "2. Upiši mjesto i dodirni ga",
      "Mobitel mora biti na istom WiFi-ju",
      "Prvo se spoji na WiFi",
      "Sada: %s",
      "Hrvatski",
      "2. Upiši naziv mreže i lozinku",
      "Mobitel ostaje na ovom WiFi-ju",
      "Obrisati?",
      "u kodu",
      "Spremljeno: %s",
      "Nema spremljenih mreža",
      "Mobitel je spojen",
      "Sad skeniraj ovo da otvoriš stranicu",
      "ili u pregledniku otvori %s",
      "Aplikacije",
      "Zvono svaki sat",
      "Noću je tiho (22:00 - 07:00)",
      "Gašenje ekrana",
      "Nikad",
      "Pali se na dodir ili kad ga podigneš",
      "Isključi",
      "Odustani",
      "Isključiti sat?",
      "Ponovno se pali tipkom PWR",
      "Noćno zatamnjenje",
      "Tajmer",
      "Kuhinjski tajmer",
      "Kreni",
      "Pauza",
      "Nastavi",
      "Poništi",
      "Stop",
      "Gotovo!",
      "Odabrana aplikacija je na početnom ekranu",
      "Libela",
      "Libela",
      "Nula",
      "Ravno",
      "Senzor pokreta nije pronađen",
      "Avatar",
      "Maše",
      "Sluša glazbu",
      "Pleše",
      "Odmara",
      "Tipka",
      "Spava",
  };
  static_assert(sizeof(kEn) / sizeof(kEn[0]) == static_cast<int>(Str::Count), "kEn");
  static_assert(sizeof(kHr) / sizeof(kHr[0]) == static_cast<int>(Str::Count), "kHr");
  const int i = static_cast<int>(id);
  return lang == Lang::Hr ? kHr[i] : kEn[i];
}

inline const char *weekday(Lang lang, int wd) {
  static const char *const kEn[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                    "Thursday", "Friday", "Saturday"};
  static const char *const kHr[] = {"Nedjelja", "Ponedjeljak", "Utorak", "Srijeda",
                                    "Četvrtak", "Petak", "Subota"};
  const int i = (wd % 7 + 7) % 7;
  return lang == Lang::Hr ? kHr[i] : kEn[i];
}

// English month name, or Croatian month in the genitive ("22. listopada").
inline const char *month(Lang lang, int m) {
  static const char *const kEn[] = {"January", "February", "March", "April",
                                    "May", "June", "July", "August",
                                    "September", "October", "November", "December"};
  static const char *const kHr[] = {"siječnja", "veljače", "ožujka", "travnja",
                                    "svibnja", "lipnja", "srpnja", "kolovoza",
                                    "rujna", "listopada", "studenoga", "prosinca"};
  const int i = (m - 1 + 12) % 12;
  return lang == Lang::Hr ? kHr[i] : kEn[i];
}

// Description of a WMO weather code.
inline const char *describe(Lang lang, int code, bool day) {
  const bool hr = lang == Lang::Hr;
  switch (code) {
  case 0: return hr ? (day ? "Sunčano" : "Vedro") : (day ? "Sunny" : "Clear");
  case 1: return hr ? "Pretežno vedro" : "Mostly clear";
  case 2: return hr ? "Djelomično oblačno" : "Partly cloudy";
  case 3: return hr ? "Oblačno" : "Cloudy";
  case 45: case 48: return hr ? "Magla" : "Fog";
  case 51: case 53: case 55: return hr ? "Rosulja" : "Drizzle";
  case 56: case 57: return hr ? "Ledena rosulja" : "Freezing drizzle";
  case 61: return hr ? "Slaba kiša" : "Light rain";
  case 63: return hr ? "Kiša" : "Rain";
  case 65: return hr ? "Jaka kiša" : "Heavy rain";
  case 66: case 67: return hr ? "Ledena kiša" : "Freezing rain";
  case 71: return hr ? "Slab snijeg" : "Light snow";
  case 73: return hr ? "Snijeg" : "Snow";
  case 75: return hr ? "Jak snijeg" : "Heavy snow";
  case 77: return hr ? "Snježna zrnca" : "Snow grains";
  case 80: return hr ? "Slabi pljuskovi" : "Light showers";
  case 81: return hr ? "Pljuskovi" : "Showers";
  case 82: return hr ? "Jaki pljuskovi" : "Heavy showers";
  case 85: case 86: return hr ? "Snježni pljuskovi" : "Snow showers";
  case 95: return hr ? "Grmljavina" : "Thunderstorm";
  case 96: case 99: return hr ? "Grmljavina s tučom" : "Thunderstorm, hail";
  default: return "";
  }
}

} // namespace i18n
