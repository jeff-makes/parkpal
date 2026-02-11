/* ParkPal – ESP32 + 7.5" tri-color e-ink
   - MODES: Parks or Countdowns
   - Web UI at http://parkpal.local/
   - Auto-migrates legacy config to new schema on boot
   - Timezone-correct countdowns for both modules
   - NEW: festive vector icons (tree, reindeer, pumpkin, ghost, cake)
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <GxEPD2_3C.h> // Correct library for tri-color display
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <vector>
#include <esp_system.h>

#include "parkpal_types.h"

// ---- WiFi & API ----
// Provisioned at runtime (AP captive portal). Stored in NVS (Preferences).
static String WIFI_SSID;
static String WIFI_PASS;
static String API_BASE_URL; // e.g. https://your-worker.your-subdomain.workers.dev (no trailing slash)

// --- WiFi diagnostics ---
static volatile uint8_t last_wifi_disconnect_reason = 0; // wifi_err_reason_t

static const char* wlStatusToStr(wl_status_t s) {
    switch (s) {
        case WL_NO_SHIELD: return "NO_SHIELD";
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

static const char* wifiReasonToStr(uint8_t r) {
    // Common ESP32/Arduino disconnect reasons. Values differ slightly across core versions;
    // keep this best-effort (unknown values still print as numbers).
    switch (r) {
        case 1: return "UNSPECIFIED";
        case 2: return "AUTH_EXPIRE";
        case 4: return "ASSOC_EXPIRE";
        case 7: return "NOT_ASSOCED";
        case 8: return "ASSOC_LEAVE";
        case 15: return "4WAY_HANDSHAKE_TIMEOUT";
        case 16: return "GROUP_KEY_TIMEOUT";
        case 23: return "8021X_AUTH_FAILED";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        default: return "UNKNOWN";
    }
}

static String normApiBaseUrl(String s) {
    s.trim();
    while (s.endsWith("/")) s.remove(s.length() - 1);
    // If a user pastes .../v1, accept it but normalize to base.
    if (s.endsWith("/v1")) s = s.substring(0, s.length() - 3);
    while (s.endsWith("/")) s.remove(s.length() - 1);
    return s;
}

static inline String apiUrl(const char* path) {
    // `path` should start with "/v1/..."
    return API_BASE_URL + path;
}
const uint32_t REFRESH_MS = 1800000; // 30 min
const uint32_t WIFI_RECONNECT_INTERVAL_MS = 30000; // Don't spam reconnect attempts
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
const uint32_t HTTP_TIMEOUT_MS = 7000; // Bounds TLS handshake + request/response
const uint32_t API_ERROR_RETRY_MS = 120000; // Retry sooner after transient API errors
const uint8_t API_FAIL_STREAK_WIFI_RESET = 3;
const uint32_t WIFI_AP_FALLBACK_AFTER_MS = 5UL * 60UL * 1000UL; // 5 min
const uint32_t FACTORY_RESET_HOLD_MS = 8000;
const int BOOT_PIN = 0; // usually GPIO0

static int last_http_code = 0;

// ---- E-paper (Waveshare ESP32 + 7.5” HD tri-color) ----
#define EPD_CS 15
#define EPD_DC 27
#define EPD_RST 26
#define EPD_BUSY 25
#define EPD_SCK 13
#define EPD_MOSI 14
using Panel = GxEPD2_750c_Z90;
const uint16_t PAGE_H = 64;
GxEPD2_3C<Panel, PAGE_H> display(Panel(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ---- Fonts ----
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
// Optional custom giant numeric font
// #include "BigDigits_120pt.h"

const int16_t BORDER_MARGIN = 75; // The new margin for all sides

const GFXfont* const LABEL_FONT = &FreeSansBold18pt7b;
#ifdef BigDigits_120pt_h
extern const GFXfont BigDigits_120pt;
const GFXfont* const NUM_FONT = &BigDigits_120pt;
#else
const GFXfont* const NUM_FONT = &FreeSansBold24pt7b;
#endif
const GFXfont* const DAYS_FONT = &FreeSansBold18pt7b;
const GFXfont* const AGE_FONT = &FreeSans12pt7b;
const GFXfont* const MSG_FONT = &FreeSansBold12pt7b;

// -------------------- Web / NVS globals --------------------
AsyncWebServer server(80);
Preferences prefs;
volatile bool config_changed = false;
volatile bool refresh_now = false;

// -------------------- Config (JSON) --------------------
static const char* DEFAULT_CONFIG = R"json({
  "mode": "parks",
  "units": "imperial",
  "trip_enabled": false,
  "trip_date": "2026-12-25",
  "parks_enabled": [6],
  "rides_by_park_ids": { "6":[0,0,0,0,0,0] },
  "rides_by_park_labels": { "6":["","","","","",""] },
  "parks_tz": "EST5EDT,M3.2.0/2,M11.1.0/2",
  "countdowns_tz": "EST5EDT,M3.2.0/2,M11.1.0/2",
  "countdowns_settings": { "show_mode": "single", "primary_id": "xmas", "cycle_every_n_refreshes": 1 },
  "countdowns": [
    { "id": "xmas", "repeat": "yearly", "month": 12, "day": 25, "label": ["CHRISTMAS COUNTDOWN"], "accent": "auto", "include_in_cycle": true, "icon": "auto" }
  ]
})json";

String loadConfigJson() {
    prefs.begin("parkpal", true);
    String s = prefs.getString("config_json", "");
    prefs.end();
    if (s.length() == 0) {
        prefs.begin("parkpal", false);
        prefs.putString("config_json", DEFAULT_CONFIG);
        prefs.end();
        return String(DEFAULT_CONFIG);
    }
    return s;
}

bool saveConfigJson(const String& s) {
    prefs.begin("parkpal", false);
    bool ok = prefs.putString("config_json", s) > 0;
    prefs.end();
    if (ok) config_changed = true;
    return ok;
}

// ------------ Small utils ------------
int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool isLeapYear(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static int daysInMonth(int year, int month) {
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31;
    if (month == 2 && isLeapYear(year)) return 29;
    return mdays[month - 1];
}

static int clampDayOfMonth(int year, int month, int day) {
    return clampi(day, 1, daysInMonth(year, month));
}

// Days since 1970-01-01 (civil day number). DST-safe for day-diff math.
static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int32_t)(era * 146097 + (int)doe - 719468);
}

static String normResort(const String& r) {
    String s = r;
    s.toLowerCase();
    if (s == "tokyo" || s == "tdr") return "tokyo";
    if (s == "california" || s == "dlr" || s == "disneyland") return "california";
    return "orlando";
}

static String regionForParkId(int parkId) {
    if (parkId == 274 || parkId == 275) return "tokyo";
    if (parkId == 16 || parkId == 17) return "california";
    return "orlando";
}

static String parkNameForId(int parkId) {
    return (parkId == 6) ? "Magic Kingdom" :
           (parkId == 5) ? "EPCOT" :
           (parkId == 7) ? "Hollywood Studios" :
           (parkId == 8) ? "Animal Kingdom" :
           (parkId == 16) ? "Disneyland" :
           (parkId == 17) ? "Disney California Adventure" :
           (parkId == 274) ? "Tokyo Disneyland" :
           (parkId == 275) ? "Tokyo DisneySea" :
           String("Park ") + parkId;
}

static String inferTripNameFromParks(const String& resort, const int* parks, int parks_n) {
    if (parks_n == 1 && parks) return parkNameForId(parks[0]);
    if (resort == "tokyo") return "Tokyo Disney";
    if (resort == "california") return "Disneyland";
    return "Disney World";
}

String normalize(const String& in) {
    String s = in;
    s.replace("’", "'");
    s.replace("‘", "'");
    s.replace("“", "\"");
    s.replace("”", "\"");
    s.replace("–", "-");
    s.replace("—", "-");
    s.replace(" / ", "/");
    s.replace(" /", "/");
    s.replace("/ ", "/");
    s.replace("™", "");
    s.replace("®", "");
    while (s.indexOf("  ") >= 0) s.replace("  ", " ");
    s.trim();
    s.toLowerCase();
    return s;
}

// -------------------- Timezone Guard Helper --------------------
struct TzGuard {
    String prev;
    TzGuard(const char* tz) {
        const char* cur = getenv("TZ");
        if (cur) prev = String(cur);
        setenv("TZ", tz, 1);
        tzset();
    }
    ~TzGuard() {
        setenv("TZ", prev.length() ? prev.c_str() : "", 1);
        tzset();
    }
};

// -------------------- RuntimeConfig Parser --------------------
bool parseConfig(RuntimeConfig& out) {
    String s = loadConfigJson();
    DynamicJsonDocument dj(32 * 1024);
    if (deserializeJson(dj, s)) return false;
    bool migrated = false;
    if (!dj.containsKey("resort")) {
        String inferred = "orlando";
        JsonArray pe0 = dj["parks_enabled"].as<JsonArray>();
        if (!pe0.isNull()) {
            for (JsonVariant v : pe0) {
                String r = regionForParkId((int)v);
                if (r == "tokyo") {
                    inferred = "tokyo";
                    break;
                }
            }
        }
        dj["resort"] = inferred;
        migrated = true;
    }
    if (!dj.containsKey("mode")) {
        dj["mode"] = "parks";
        migrated = true;
    }
    if (!dj.containsKey("parks_tz")) {
        dj["parks_tz"] = "EST5EDT,M3.2.0/2,M11.1.0/2";
        migrated = true;
    }
    if (!dj.containsKey("countdowns_tz")) {
        dj["countdowns_tz"] = "EST5EDT,M3.2.0/2,M11.1.0/2";
        migrated = true;
    }
    if (!dj.containsKey("countdowns_settings")) {
        JsonObject cs = dj.createNestedObject("countdowns_settings");
        cs["show_mode"] = "single";
        cs["primary_id"] = "";
        cs["cycle_every_n_refreshes"] = 1;
        migrated = true;
    }
    if (!dj.containsKey("countdowns")) {
        dj.createNestedArray("countdowns");
        migrated = true;
    }
    out.mode = dj["mode"].as<String>();
    {
        String raw = dj["resort"] | "orlando";
        String norm = normResort(raw);
        out.resort = norm;
        if (raw != norm) {
            dj["resort"] = norm;
            migrated = true;
        }
    }
    out.parks_tz = dj["parks_tz"].as<String>();
    out.countdowns_tz = dj["countdowns_tz"].as<String>();
    JsonObject cs = dj["countdowns_settings"];
    out.countdownSettings.show_mode = cs["show_mode"] | "single";
    out.countdownSettings.primary_id = cs["primary_id"] | "";
    out.countdownSettings.cycle_every_n_refreshes = cs["cycle_every_n_refreshes"] | 1;
    out.countdowns.clear();
    JsonArray cd = dj["countdowns"].as<JsonArray>();
    if (!cd.isNull()) {
        for (JsonVariant v : cd) {
            CountdownItem item;
            item.id = v["id"] | "";
            item.repeat = v["repeat"] | "yearly";
            item.year = v["year"] | 0;
            item.month = v["month"] | 0;
            item.day = v["day"] | 0;
            item.birth_year = v["birth_year"] | 0;
            item.accent = v["accent"] | "auto";
            item.include_in_cycle = v["include_in_cycle"] | true;
            item.icon = v["icon"] | "auto";
            JsonArray lbl = v["label"].as<JsonArray>();
            if (!lbl.isNull()) {
                for (int i = 0; i < lbl.size() && i < 4; i++) item.label[i] = lbl[i].as<String>();
            }
            out.countdowns.push_back(item);
        }
    }
    out.metric = (String(dj["units"] | "metric") == "metric");
    out.trip_enabled = dj["trip_enabled"] | true;
    out.trip_date = String(dj["trip_date"] | "2026-12-25");
    out.trip_name = String(dj["trip_name"] | "");
    out.parks_n = 0;
    JsonArray pe = dj["parks_enabled"].as<JsonArray>();
    if (!pe.isNull())
        for (JsonVariant v : pe)
            if (out.parks_n < 4) out.parks[out.parks_n++] = (int)v;
    if (out.parks_n == 0) {
        out.parks[0] = 6;
        out.parks_n = 1;
    }

    // If `trip_name` was never set (older configs), seed a stable default.
    // If the user explicitly clears it to blank, keep it blank and infer at render-time.
    if (!dj.containsKey("trip_name")) {
        String inferred = inferTripNameFromParks(out.resort, out.parks, out.parks_n);
        dj["trip_name"] = inferred;
        out.trip_name = inferred;
        migrated = true;
    }
    for (int i = 0; i < out.parks_n; i++) {
        int pid = out.parks[i];
        JsonArray ids = dj["rides_by_park_ids"][String(pid)].as<JsonArray>();
        JsonArray lbl = dj["rides_by_park_labels"][String(pid)].as<JsonArray>();
        JsonArray leg = dj["rides_by_park"][String(pid)].as<JsonArray>(); // legacy labels
        for (int r = 0; r < 6; r++) {
            out.rideIds[i][r] = (ids.isNull() || r >= (int)ids.size()) ? 0 : (int)ids[r];
            out.rideLabels[i][r] = (lbl.isNull() || r >= (int)lbl.size()) ? "" : String(lbl[r] | "");
            out.legacyNames[i][r] = (leg.isNull() || r >= (int)leg.size()) ? "" : String(leg[r] | "");
        }
    }
    if (migrated) {
        String outStr;
        serializeJson(dj, outStr);
        saveConfigJson(outStr);
    }
    return true;
}

// -------------------- HTML UI --------------------
#include "html.h"
#include "setup_html.h"

// -------------------- Provisioning / Captive Portal --------------------
DNSServer dnsServer;
bool in_setup_mode = false;
String setup_ap_ssid;
String setup_ap_pass;
static bool pending_restart = false;
static unsigned long restart_at_ms = 0;
static bool just_provisioned = false;

static void scheduleRestart(uint32_t delayMs) {
    pending_restart = true;
    restart_at_ms = millis() + delayMs;
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            last_wifi_disconnect_reason = info.wifi_sta_disconnected.reason;
            Serial.printf("WiFi event: STA_DISCONNECTED reason=%u (%s)\n", (unsigned)last_wifi_disconnect_reason, wifiReasonToStr(last_wifi_disconnect_reason));
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("WiFi event: STA_CONNECTED");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("WiFi event: GOT_IP %s\n", WiFi.localIP().toString().c_str());
            kickNTP();
            break;
        default:
            break;
    }
}

static void loadProvisioningKeys() {
    prefs.begin("parkpal", true);
    WIFI_SSID = prefs.getString("wifi_ssid", "");
    WIFI_PASS = prefs.getString("wifi_pass", "");
    API_BASE_URL = normApiBaseUrl(prefs.getString("api_base_url", ""));
    // Used to make first-time setup less frustrating: if the user just provisioned
    // and we still can't connect, immediately return to setup mode on boot.
    just_provisioned = prefs.getBool("just_provisioned", false);
    prefs.end();
    WIFI_SSID.trim();
    API_BASE_URL.trim();
}

static void saveProvisioningKeys(const String& ssid, const String& pass, const String& baseUrl) {
    prefs.begin("parkpal", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.putString("api_base_url", normApiBaseUrl(baseUrl));
    prefs.putBool("just_provisioned", true);
    prefs.end();
    loadProvisioningKeys();
}

static void clearJustProvisionedFlag() {
    if (!just_provisioned) return;
    prefs.begin("parkpal", false);
    prefs.remove("just_provisioned");
    prefs.end();
    just_provisioned = false;
}

static void wipeProvisioningKeys(bool wipeConfigJson) {
    prefs.begin("parkpal", false);
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
    prefs.remove("api_base_url");
    prefs.remove("just_provisioned");
    if (wipeConfigJson) prefs.remove("config_json");
    prefs.end();
    loadProvisioningKeys();
}

static bool isProvisioned() {
    return WIFI_SSID.length() > 0 && API_BASE_URL.length() > 0;
}

// -------------------- Wi-Fi / NTP --------------------
void connectWiFi() {
    if (WIFI_SSID.length() == 0) return;
    Serial.printf("WiFi: begin connect (ssid_len=%u pass_len=%u)\n", (unsigned)WIFI_SSID.length(), (unsigned)WIFI_PASS.length());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (uint32_t)(millis() - t0) < WIFI_CONNECT_TIMEOUT_MS) delay(200);
    Serial.printf("WiFi: connect result status=%d (%s)\n", (int)WiFi.status(), wlStatusToStr(WiFi.status()));
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("WiFi: last disconnect reason=%u (%s)\n", (unsigned)last_wifi_disconnect_reason, wifiReasonToStr(last_wifi_disconnect_reason));
    }
    if (WiFi.status() == WL_CONNECTED) clearJustProvisionedFlag();
}

bool ensureWiFiConnected(uint32_t timeoutMs = 0) {
    static unsigned long lastAttemptMs = 0;
    if (WiFi.status() == WL_CONNECTED) return true;
    if (WIFI_SSID.length() == 0) return false;

    const unsigned long now = millis();
    if (lastAttemptMs == 0 || (uint32_t)(now - lastAttemptMs) >= WIFI_RECONNECT_INTERVAL_MS) {
        lastAttemptMs = now;
        WiFi.disconnect(false);
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
        Serial.printf("WiFi: reconnect attempt (status=%d)\n", (int)WiFi.status());
    }

    if (timeoutMs == 0) return false;

    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (uint32_t)(millis() - start) < timeoutMs) {
        delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) clearJustProvisionedFlag();
    return WiFi.status() == WL_CONNECTED;
}

void kickNTP() {
    // Re-assert SNTP servers after reconnects; non-blocking.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void initNTP() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    TzGuard guard("EST5EDT,M3.2.0/2,M11.1.0/2"); // default TZ for initial sync
    time_t now = 0;
    int tries = 0;
    while (now < 1700000000 && tries < 150) {
        delay(100);
        time(&now);
        tries++;
    }
}

bool parseISODateYMD(const String& iso, int& year, int& month, int& day) {
    if (iso.length() < 10) return false;
    year = iso.substring(0, 4).toInt();
    month = clampi(iso.substring(5, 7).toInt(), 1, 12);
    day = clampi(iso.substring(8, 10).toInt(), 1, 31);
    day = clampDayOfMonth(year, month, day);
    return year >= 1970;
}

// -------------------- Time Calculation --------------------
bool daysToDateInTz(const String& isoDate, const char* tz, int& outDays) {
    TzGuard guard(tz);
    time_t now;
    time(&now);
    if (now < 1700000000) return false;
    struct tm* lt = localtime(&now);
    if (!lt) return false;
    const int today_y = lt->tm_year + 1900;
    const int today_m = lt->tm_mon + 1;
    const int today_d = lt->tm_mday;

    int y, m, d;
    if (!parseISODateYMD(isoDate, y, m, d)) return false;
    d = clampDayOfMonth(y, m, d);

    int32_t diff = daysFromCivil(y, (unsigned)m, (unsigned)d) - daysFromCivil(today_y, (unsigned)today_m, (unsigned)today_d);
    outDays = diff < 0 ? 0 : (int)diff;
    return true;
}

bool computeDaysToEvent(const CountdownItem& c, const char* tz, int& outDays, int& outTurnsAge) {
    // "once" past events -> show DONE! (days = 0)
    TzGuard guard(tz);
    time_t now;
    time(&now);
    if (now < 1700000000) {
        outDays = -2;
        return false;
    }
    struct tm* lt = localtime(&now);
    if (!lt) {
        outDays = -2;
        return false;
    }
    int today_y = lt->tm_year + 1900, today_m = lt->tm_mon + 1, today_d = lt->tm_mday;
    outDays = 0;
    outTurnsAge = 0;
    if (c.repeat == "once") {
        int y = (c.year > 0) ? c.year : today_y;
        int m = clampi(c.month, 1, 12);
        int d = clampDayOfMonth(y, m, clampi(c.day, 1, 31));
        int32_t diff = daysFromCivil(y, (unsigned)m, (unsigned)d) - daysFromCivil(today_y, (unsigned)today_m, (unsigned)today_d);
        outDays = diff < 0 ? 0 : (int)diff;
        return true;
    } else { // yearly
        int target_y = today_y;
        int m = clampi(c.month, 1, 12);
        int d = clampi(c.day, 1, 31);

        // Normalize invalid dates (e.g., Feb 29 on non-leap years -> Mar 1).
        auto normalizeMonthDayForYear = [&](int year, int& ioM, int& ioD) {
            if (ioM == 2 && ioD == 29 && !isLeapYear(year)) {
                ioM = 3;
                ioD = 1;
            } else {
                ioD = clampDayOfMonth(year, ioM, ioD);
            }
        };

        normalizeMonthDayForYear(target_y, m, d);
        if (m < today_m || (m == today_m && d < today_d)) {
            target_y++;
            m = clampi(c.month, 1, 12);
            d = clampi(c.day, 1, 31);
            normalizeMonthDayForYear(target_y, m, d);
        }
        if (c.birth_year > 0) outTurnsAge = target_y - c.birth_year;
        int32_t diff = daysFromCivil(target_y, (unsigned)m, (unsigned)d) - daysFromCivil(today_y, (unsigned)today_m, (unsigned)today_d);
        outDays = diff < 0 ? 0 : (int)diff;
        return true;
    }
}

// -------------------- HTTP helpers --------------------
bool httpPostJson(const char* url, const String& body, DynamicJsonDocument& outDoc) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    last_http_code = code;
    if (code != 200) {
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();
    return (deserializeJson(outDoc, payload) == DeserializationError::Ok);
}

bool httpGetJson(const String& url, DynamicJsonDocument& outDoc) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int code = http.GET();
    last_http_code = code;
    if (code != 200) {
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();
    return (deserializeJson(outDoc, payload) == DeserializationError::Ok);
}

bool fetchSummaryForPark(int parkId, bool metricUnits, const int rideIds[6], DynamicJsonDocument &doc) {
    if (API_BASE_URL.length() == 0) return false;
    DynamicJsonDocument bodyDoc(1024);
    bodyDoc["park"] = parkId;
    bodyDoc["units"] = metricUnits ? "metric" : "imperial";
    JsonArray favs = bodyDoc.createNestedArray("favorite_ride_ids");
    for (int i = 0; i < 6; i++) {
        if (rideIds[i] > 0) favs.add(rideIds[i]);
    }
    String body;
    serializeJson(bodyDoc, body);
    const String url = apiUrl("/v1/summary");
    return httpPostJson(url.c_str(), body, doc);
}

// -------------------- Drawing helpers --------------------
void drawText(int16_t x, int16_t y, const String& s, const GFXfont* f, uint16_t color) {
    display.setFont(f);
    display.setTextColor(color);
    display.setCursor(x, y);
    display.print(s);
}

int16_t textWidth(const String& s, const GFXfont* f) {
    int16_t x1, y1;
    uint16_t w, h;
    display.setFont(f);
    display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return w;
}

String clipToWidth(const String& s, const GFXfont* f, int16_t maxW, bool ellipsis = true) {
    if (maxW <= 0) return "";
    if (textWidth(s, f) <= maxW) return s;
    if (!ellipsis) {
        String out = s;
        while (out.length() > 1 && textWidth(out, f) > maxW) out.remove(out.length() - 1);
        return out;
    }
    const String dots = "...";
    const int16_t dotsW = textWidth(dots, f);
    if (dotsW >= maxW) return "";
    String out = s;
    while (out.length() > 1 && textWidth(out + dots, f) > maxW) out.remove(out.length() - 1);
    return out + dots;
}

const GFXfont* pickLargestFontThatFits(const String& s, int16_t maxW, const GFXfont* a, const GFXfont* b, const GFXfont* c) {
    if (textWidth(s, a) <= maxW) return a;
    if (textWidth(s, b) <= maxW) return b;
    return c;
}

void drawRight(int16_t rightX, int16_t baselineY, const String& s, const GFXfont* f, uint16_t color) {
    drawText(rightX - textWidth(s, f), baselineY, s, f, color);
}

inline void thickH(int x1, int y, int x2, uint16_t c) {
    display.drawLine(x1, y, x2, y, c);
    display.drawLine(x1, y + 1, x2, y + 1, c);
}

inline void thickV(int x, int y1, int y2, uint16_t c) {
    display.drawLine(x, y1, x, y2, c);
    display.drawLine(x + 1, y1, x + 1, y2, c);
}

void drawDegreeMark(int16_t cx, int16_t cy, int16_t outerR, uint16_t color) {
    // E-ink can render 1px outlines very faintly; use a filled ring for contrast.
    outerR = max<int16_t>(2, outerR);
    display.fillCircle(cx, cy, outerR, color);
    int16_t innerR = outerR - 2;
    if (innerR > 0) display.fillCircle(cx, cy, innerR, GxEPD_WHITE);
}

void drawMickeySilhouette(int cx, int cy, int r, uint16_t color) {
    display.drawCircle(cx, cy, r, color);
    display.fillCircle(cx, cy, r - 2, color);
    int er = r * 0.45;
    display.fillCircle(cx - r, cy - r, er, color);
    display.fillCircle(cx + r, cy - r, er, color);
}

void drawCenterLine(int16_t baselineY, const String& s, const GFXfont* f, uint16_t color) {
    display.setFont(f);
    int16_t x1, y1;
    uint16_t w, h;
    int16_t availableWidth = display.width() - 2 * BORDER_MARGIN;
    String clipped_s = clipToWidth(s, f, availableWidth, false);
    display.getTextBounds(clipped_s, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (display.width() - (int16_t)w) / 2;
    display.setTextColor(color);
    display.setCursor(x, baselineY);
    display.print(clipped_s);
}


int16_t lineHeight(const GFXfont* f) {
    display.setFont(f);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("Hg", 0, 0, &x1, &y1, &w, &h);
    return (int16_t)h + 6;
}

// ====== FESTIVE ICONS =================================================

IconKind pickIcon(const CountdownItem& c) {
    // Explicit override
    if (c.icon == "tree") return ICON_TREE;
    if (c.icon == "reindeer") return ICON_REINDEER;
    if (c.icon == "pumpkin") return ICON_PUMPKIN;
    if (c.icon == "ghost") return ICON_GHOST;
    if (c.icon == "cake") return ICON_CAKE;
    if (c.icon == "none") return ICON_NONE;
    // AUTO: infer by label/date
    String labels = (c.label[0] + " " + c.label[1] + " " + c.label[2] + " " + c.label[3]);
    String L = labels;
    L.toLowerCase();
    if (L.indexOf("christmas") >= 0) return ICON_TREE;
    if (L.indexOf("halloween") >= 0) return ICON_PUMPKIN;
    if (L.indexOf("ghost") >= 0) return ICON_GHOST;
    if (L.indexOf("reindeer") >= 0) return ICON_REINDEER;
    if (L.indexOf("birthday") >= 0 || c.birth_year > 0) return ICON_CAKE;
    if (c.repeat == "yearly") {
        if (c.month == 12 && c.day >= 20 && c.day <= 26) return ICON_TREE;
        if (c.month == 10 && c.day >= 25 && c.day <= 31) return ICON_PUMPKIN;
        if (c.month == 1 && c.day == 1) return ICON_REINDEER; // playful
    }
    return ICON_NONE;
}

// helper
void fillTriangleI(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    display.fillTriangle(x0, y0, x1, y1, x2, y2, c);
}

// TREE
void drawIconTree(int cx, int cy, int s, uint16_t c) {
    int w = s, h = s;
    // three stacked triangles
    fillTriangleI(cx - w * 0.35, cy - h * 0.35, cx + w * 0.35, cy - h * 0.35, cx, cy - h * 0.65, c);
    fillTriangleI(cx - w * 0.45, cy - h * 0.10, cx + w * 0.45, cy - h * 0.10, cx, cy - h * 0.45, c);
    fillTriangleI(cx - w * 0.55, cy + h * 0.15, cx + w * 0.55, cy + h * 0.15, cx, cy - h * 0.20, c);
    // trunk
    int tw = w * 0.14, th = h * 0.18;
    display.fillRect(cx - tw / 2, cy + h * 0.15, tw, th, c);
}

// REINDEER (minimal)
void drawIconReindeer(int cx, int cy, int s, uint16_t c, uint16_t noseC) {
    int r = s / 4;
    display.fillCircle(cx, cy, r, c); // head
    // antlers
    for (int i = 0; i < 3; i++) {
        display.drawLine(cx - r, cy - r + i * 3, cx - r - s * 0.25, cy - r - s * 0.10 + i * 2, c);
        display.drawLine(cx + r, cy - r + i * 3, cx + r + s * 0.25, cy - r - s * 0.10 + i * 2, c);
    }
    display.fillCircle(cx, cy + r * 0.9, r * 0.35, noseC); // nose
}

// PUMPKIN (three overlapping circles + stem + grooves)
void drawIconPumpkin(int cx, int cy, int s, uint16_t c) {
    int r = s * 0.28;
    display.fillCircle(cx - r, cy, r, c);
    display.fillCircle(cx, cy, r * 1.15, c);
    display.fillCircle(cx + r, cy, r, c);
    // stem
    display.fillRect(cx - s * 0.05, cy - r * 1.6, s * 0.10, r * 0.9, c);
    // grooves (thin vertical lines knocked out)
    int bodyW = r * 3;
    for (int i = -2; i <= 2; i++) {
        int x = cx + i * (bodyW / 10);
        display.drawFastVLine(x, cy - r * 1.15, r * 2.3, GxEPD_WHITE);
    }
}

// GHOST
void drawIconGhost(int cx, int cy, int s, uint16_t c) {
    int r = s / 2;
    display.fillCircle(cx, cy - r * 0.3, r * 0.7, c); // head
    display.fillRect(cx - r * 0.7, cy - r * 0.3, r * 1.4, r * 0.9, c); // body
    // scalloped bottom
    for (int i = -2; i <= 2; i++) {
        display.fillCircle(cx + i * (r * 0.5), cy + r * 0.45, r * 0.3, c);
    }
    // eyes (white cutouts)
    display.fillCircle(cx - r * 0.25, cy - r * 0.25, r * 0.10, GxEPD_WHITE);
    display.fillCircle(cx + r * 0.25, cy - r * 0.25, r * 0.10, GxEPD_WHITE);
}

// CAKE
void drawIconCake(int cx, int cy, int s, uint16_t c, uint16_t accent) {
    int w = s, h = s * 0.6;
    int x = cx - w / 2, y = cy - h / 2;
    display.fillRect(x, y + h * 0.35, w, h * 0.65, c); // base
    display.fillRect(x, y + h * 0.25, w, h * 0.12, accent); // frosting stripe
    // candle
    int cw = w * 0.08, ch = h * 0.35;
    display.fillRect(cx - cw / 2, y, cw, ch, c);
    // flame
    display.fillCircle(cx, y - h * 0.02, cw, accent);
}

void drawIcon(IconKind k, int x, int y, int size) {
    if (k == ICON_NONE) return;
    int cx = x + size / 2, cy = y + size / 2;
    switch (k) {
    case ICON_TREE:
        drawIconTree(cx, cy, size, GxEPD_BLACK);
        break;
    case ICON_REINDEER:
        drawIconReindeer(cx, cy, size, GxEPD_BLACK, GxEPD_RED);
        break;
    case ICON_PUMPKIN:
        drawIconPumpkin(cx, cy, size, GxEPD_BLACK);
        break;
    case ICON_GHOST:
        drawIconGhost(cx, cy, size, GxEPD_BLACK);
        break;
    case ICON_CAKE:
        drawIconCake(cx, cy, size, GxEPD_BLACK, GxEPD_RED);
        break;
    default:
        break;
    }
}

// =====================================================================
// -------------------- Render: Parks --------------------
String parks_lastFrameKey;
void renderParks(const DynamicJsonDocument& doc, const int rideIds[6], const String rideLabels[6], const String& parkName, bool metricUnits, bool showTrip, const String& tripISO, const String& tripName, const String legacyFallback[6], const char* parksTz) {
    int temp = doc["weather"]["temp"] | 0;
    String desc = String(doc["weather"]["desc"] | "—");
    struct Row {
        String name;
        bool open;
        int wait;
    };
    Row rows[6];
    int count = 0;
    JsonArrayConst ridesJson = doc["park"]["rides"].as<JsonArrayConst>();
    for (int s = 0; s < 6; s++) {
        int dId = rideIds[s];
        String dLbl = rideLabels[s];
        String legLbl = legacyFallback[s];
        if (dId == 0 && dLbl.length() == 0 && legLbl.length() == 0) continue;
        bool found = false;
        if (!ridesJson.isNull()) {
            for (JsonVariantConst ri : ridesJson) {
                int apiId = ri["id"] | 0;
                String apiName = String(ri["name"] | "—");
                bool isMatch = false;
                if (dId > 0 && apiId == dId) isMatch = true;
                else if (dId == 0) {
                    String want = dLbl.length() ? dLbl : legLbl;
                    if (normalize(apiName) == normalize(want)) isMatch = true;
                }
                if (isMatch) {
                    if (apiName.indexOf("Single Rider") >= 0) continue;
                    rows[count++] = {apiName, (bool)(ri["is_open"] | false), (int)(ri["wait_time"] | 0)};
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            String name = dLbl.length() ? dLbl : legLbl;
            if (name.length() > 0) rows[count++] = {name, false, -1};
        }
        if (count >= 6) break;
    }
    int days = 0;
    bool haveTime = false;
    if (showTrip) haveTime = daysToDateInTz(tripISO, parksTz, days);
    String key = parkName + "|" + temp + "|" + desc + "|" + (showTrip ? String(days) : "-") + "|" + tripName;
    for (int i = 0; i < count; i++) key += "|" + rows[i].name + "|" + (rows[i].open ? "1" : "0") + "|" + rows[i].wait;
    if (key == parks_lastFrameKey) return;
    parks_lastFrameKey = key;
    
    const int16_t W = display.width(), H = display.height(), M = BORDER_MARGIN, MID_X = W / 2;
    
    const GFXfont* titleFont = &FreeSansBold12pt7b;
    const GFXfont* subContentFont = &FreeSans12pt7b;
    const GFXfont* largeNumFont = &FreeSansBold24pt7b;
    const GFXfont* largeDaysFont = &FreeSansBold18pt7b;

    int16_t titleHeight = lineHeight(titleFont);
    int16_t numHeight = lineHeight(largeNumFont);
    int16_t contentPadding = 20;
    
    // Header height covers the title row + one content row in each column.
    int16_t maxHeaderContentHeight = titleHeight + contentPadding + numHeight;
    int16_t dynamicHeaderHeight = M + maxHeaderContentHeight + 20;

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        thickV(MID_X, M, dynamicHeaderHeight, GxEPD_BLACK);
        thickH(M, dynamicHeaderHeight, W - M, GxEPD_BLACK);

        int16_t currentY;

        // --- Left Column: Trip Countdown ---
        currentY = M + titleHeight;
        const int16_t leftMaxW = (MID_X - 10) - M;
        if (showTrip) {
            if (haveTime) {
                const String untilLine = String(days) + " DAYS UNTIL";
                drawText(M, currentY, clipToWidth(untilLine, titleFont, leftMaxW, true), titleFont, GxEPD_BLACK);
            } else {
                drawText(M, currentY, "TRIP COUNTDOWN", titleFont, GxEPD_BLACK);
            }
        }

        const int16_t contentY = currentY + numHeight + contentPadding;
        if (showTrip) {
            if (haveTime) {
                const String effectiveTripName = tripName.length() ? tripName : "My Trip";
                const GFXfont* tripFont = pickLargestFontThatFits(effectiveTripName, leftMaxW, largeNumFont, largeDaysFont, titleFont);
                drawText(M, contentY, clipToWidth(effectiveTripName, tripFont, leftMaxW, true), tripFont, GxEPD_BLACK);
            } else {
                drawText(M, contentY, "—", largeNumFont, GxEPD_RED);
            }
        } else {
            drawMickeySilhouette(M + (MID_X - M) / 2, M + maxHeaderContentHeight / 2, 40, GxEPD_BLACK);
        }

        // --- Right Column: Weather ---
        int16_t c2X = MID_X + 25; // Shift closer to the middle line
        currentY = M + titleHeight;
        drawText(c2X, currentY, "WEATHER", titleFont, GxEPD_BLACK);
        
        currentY = contentY;
        // NOTE: FreeSans GFX fonts are ASCII-only; draw the degree symbol manually.
        const String tempNum = String(temp);
        const String unit = metricUnits ? "C" : "F";
        drawText(c2X, currentY, tempNum, largeNumFont, GxEPD_BLACK);

        // Compute bounds for positioning the degree symbol near the top-right of the number.
        int16_t bx, by;
        uint16_t bw, bh;
        display.setFont(largeNumFont);
        display.getTextBounds(tempNum, c2X, currentY, &bx, &by, &bw, &bh);
        int16_t degreeR = (int16_t)max(3, min(7, (int)(bh / 6)));
        int16_t degreeCx = bx + (int16_t)bw + degreeR + 3;
        int16_t degreeCy = by + degreeR + 2;
        drawDegreeMark(degreeCx, degreeCy, degreeR, GxEPD_BLACK);

        int16_t unitX = degreeCx + degreeR + 4;
        drawText(unitX, currentY, unit, largeNumFont, GxEPD_BLACK);

        int16_t descX = unitX + textWidth(unit, largeNumFont) + 10;
        int16_t descMaxW = (W - M) - descX;
        drawText(descX, currentY, clipToWidth(desc, subContentFont, descMaxW, true), subContentFont, GxEPD_BLACK);
        
        // --- Ride List ---
        int16_t listHeaderY = dynamicHeaderHeight + 30;
        drawText(M, listHeaderY, parkName, titleFont, GxEPD_RED);

        const int16_t listTop = listHeaderY + lineHeight(titleFont) + 5;
        const int16_t rowH = 42; // Tighten up row height
        const int16_t waitColR = W - M;
        int16_t y = listTop;
        for (int i = 0; i < count; i++) {
            if (y > (H - M)) break;
            int16_t maxW = (W - M - M) - 140;
            String name = clipToWidth(rows[i].name, subContentFont, maxW, true);
            drawText(M, y, name, subContentFont, GxEPD_BLACK);
            
            if (rows[i].wait == -1) drawRight(waitColR, y, "Unavailable", titleFont, GxEPD_RED);
            else if (rows[i].open) drawRight(waitColR, y, String(rows[i].wait) + " min", titleFont, GxEPD_BLACK);
            else drawRight(waitColR, y, "Closed", titleFont, GxEPD_RED);
            
            if (i < count - 1) {
                thickH(M, y + 10, W - M, GxEPD_BLACK);
            }
            y += rowH;
        }
    } while (display.nextPage());
}


// -------------------- Render: Countdowns & Messages --------------------
String countdowns_lastFrameKey;

void renderMessage(const String& msg, const GFXfont* font) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawCenterLine(display.height() / 2, msg, font, GxEPD_BLACK);
    } while (display.nextPage());
}

void renderCountdowns(const CountdownItem& active, int days, int turnsAge) {
    // Redundant frame skip
    String key = active.id + "|" + days + "|" + turnsAge + "|" + active.icon;
    for (int i = 0; i < 4; i++) key += "|" + active.label[i];
    if (key == countdowns_lastFrameKey) return;
    countdowns_lastFrameKey = key;
    const int16_t W = display.width(), H = display.height(), M = BORDER_MARGIN;
    const int16_t GAP_BEFORE_NUMBER = 18, GAP_BEFORE_AGE = 10;
    int labelLines = 0;
    for (int i = 0; i < 4; i++)
        if (active.label[i].length()) labelLines++;
    int16_t hLabels = labelLines * lineHeight(LABEL_FONT);
    int16_t hNumber = lineHeight(NUM_FONT);
    int16_t hDays = (days > 0) ? lineHeight(DAYS_FONT) : 0;
    int16_t hAge = (days == 0 && turnsAge > 0) ? lineHeight(AGE_FONT) : 0;
    int16_t total = hLabels + (labelLines ? GAP_BEFORE_NUMBER : 0) + (days == 0 ? (hNumber + (hAge ? (GAP_BEFORE_AGE + hAge) : 0)) : (hNumber + hDays));
    const int16_t y0 = (H - total) / 2; // stable baseline each page
    // ---- Icon placement (above labels if space, else top-right) ----
    IconKind icon = pickIcon(active);
    const int16_t PAD = BORDER_MARGIN;
    int iconSize = 120;
    int iconX = 0, iconY = 0;
    int16_t topFree = y0 - PAD; // space from top to first content line
    if (icon != ICON_NONE && topFree > 80) {
        iconSize = min(iconSize, (int)topFree - PAD);
        if (iconSize < 56) iconSize = 56;
        iconX = (W - iconSize) / 2;
        iconY = std::max<int>((int)PAD, (int)(y0 - PAD - iconSize));
    } else if (icon != ICON_NONE) {
        iconSize = 96;
        iconX = W - iconSize - PAD;
        iconY = PAD;
    }
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        // Draw icon first, consistent each page
        if (icon != ICON_NONE) drawIcon(icon, iconX, iconY, iconSize);
        int16_t y = y0;
        // Labels
        for (int i = 0; i < 4; i++) {
            String line = active.label[i];
            if (!line.length()) continue;
            drawCenterLine(y, line, LABEL_FONT, GxEPD_BLACK);
            y += lineHeight(LABEL_FONT);
        }
        if (labelLines) y += GAP_BEFORE_NUMBER;
        if (days == 0) {
            const char* msg = (active.repeat == "once") ? "DONE!" : "TODAY!";
            drawCenterLine(y, msg, NUM_FONT, GxEPD_RED);
            y += lineHeight(NUM_FONT);
            if (hAge) {
                y += GAP_BEFORE_AGE;
                drawCenterLine(y, "turns " + String(turnsAge), AGE_FONT, GxEPD_BLACK);
            }
        } else {
            uint16_t numColor = GxEPD_BLACK;
            if (active.accent == "red" || (active.accent == "auto" && days <= 3)) numColor = GxEPD_RED;
            String dayStr = String(days);
            drawCenterLine(y, dayStr, NUM_FONT, numColor);
            y += lineHeight(NUM_FONT);
            drawCenterLine(y, "DAYS", DAYS_FONT, GxEPD_BLACK);
        }
    } while (display.nextPage());
}

// --- FORWARD DECLARATIONS ---
bool resolveParkSlotsToIds(int parkId, JsonDocument& cfgDoc);
void migrateResolveIdsIfNeeded();

// -------------------- Web endpoints --------------------
void startWeb() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * req) {
        req->send_P(200, "text/html; charset=utf-8", in_setup_mode ? SETUP_HTML : INDEX_HTML);
    });
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest * req) {
        String s = loadConfigJson();
        req->send(200, "application/json", s);
    });
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest * req) {}, NULL, [](AsyncWebServerRequest * req, uint8_t * data, size_t len, size_t index, size_t total) {
        String* body = (String*)req->_tempObject;
        if (index == 0) {
            delete body;
            body = new String();
            body->reserve(total);
            req->_tempObject = body;
        }
        if (!body) {
            req->send(500, "text/plain", "ERR");
            return;
        }
        body->concat((const char*)data, len);
        if (index + len == total) {
            const bool ok = saveConfigJson(*body);
            delete body;
            req->_tempObject = nullptr;
            req->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "ERR");
        }
    });
    server.on("/api/refresh", HTTP_POST, [](AsyncWebServerRequest * req) {
        refresh_now = true;
        req->send(200, "text/plain", "OK");
    });
    server.on("/api/rides", HTTP_GET, [](AsyncWebServerRequest * req) {
        if (!req->hasParam("park")) {
            req->send(400, "application/json", "{\"error\":\"missing park\"}");
            return;
        }
        if (API_BASE_URL.length() == 0) {
            req->send(503, "application/json", "{\"error\":\"unprovisioned\"}");
            return;
        }
        String park = req->getParam("park")->value();
        String url = apiUrl("/v1/rides") + "?park=" + park;
        DynamicJsonDocument doc(24 * 1024);
        if (httpGetJson(url, doc)) {
            String out;
            serializeJson(doc, out);
            req->send(200, "application/json", out);
        } else {
            req->send(502, "application/json", "{\"error\":\"upstream\"}");
        }
    });

    // Provisioning endpoints (used by captive portal setup page)
    server.on("/api/provision", HTTP_GET, [](AsyncWebServerRequest * req) {
        DynamicJsonDocument doc(1024);
        doc["provisioned"] = isProvisioned();
        doc["wifi_ssid"] = WIFI_SSID;
        doc["api_base_url"] = API_BASE_URL;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest * req) {}, NULL,
      [](AsyncWebServerRequest * req, uint8_t * data, size_t len, size_t index, size_t total) {
        String* body = (String*)req->_tempObject;
        if (index == 0) {
            delete body;
            body = new String();
            body->reserve(total);
            req->_tempObject = body;
        }
        if (!body) {
            req->send(500, "application/json", "{\"error\":\"internal_error\"}");
            return;
        }
        body->concat((const char*)data, len);
        if (index + len != total) return;

        DynamicJsonDocument doc(2048);
        if (deserializeJson(doc, *body) != DeserializationError::Ok) {
            delete body;
            req->_tempObject = nullptr;
            req->send(400, "application/json", "{\"error\":\"bad_request\",\"details\":\"invalid JSON\"}");
            return;
        }
        String ssid = String(doc["wifi_ssid"] | "");
        String pass = String(doc["wifi_pass"] | "");
        String api = String(doc["api_base_url"] | "");
        ssid.trim();
        pass.trim();
        api = normApiBaseUrl(api);
        if (ssid.length() == 0) {
            delete body;
            req->_tempObject = nullptr;
            req->send(400, "application/json", "{\"error\":\"bad_request\",\"details\":\"missing wifi_ssid\"}");
            return;
        }
        if (api.length() == 0 || !api.startsWith("http")) {
            delete body;
            req->_tempObject = nullptr;
            req->send(400, "application/json", "{\"error\":\"bad_request\",\"details\":\"missing api_base_url\"}");
            return;
        }
        saveProvisioningKeys(ssid, pass, api);
        delete body;
        req->_tempObject = nullptr;
        req->send(200, "application/json", "{\"ok\":true}");
        // Restarting immediately inside an async handler can cause lwIP asserts on some setups.
        // Schedule the restart from the main loop to let the response flush cleanly.
        scheduleRestart(750);
    });

    // Captive portal helpers (some clients probe these)
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest * req) { req->redirect("/"); });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest * req) { req->redirect("/"); });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * req) { req->send(204); });

    server.onNotFound([](AsyncWebServerRequest *req) {
        if (in_setup_mode) {
            req->send_P(200, "text/html; charset=utf-8", SETUP_HTML);
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });
    server.begin();
}

// -------------------- Setup / Loop --------------------
unsigned long lastTick = 0;
int parkIndex = 0;
int countdownCycleIndex = 0;
int countdownRefreshCounter = 0;
uint8_t api_fail_streak = 0;
unsigned long wifi_disconnected_since_ms = 0;
unsigned long boot_press_start_ms = 0;

static String randomAlphaNum(size_t n) {
    const char* alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789abcdefghjkmnpqrstuvwxyz";
    const size_t L = strlen(alphabet);
    String out;
    out.reserve(n);
    for (size_t i = 0; i < n; i++) {
        uint32_t r = esp_random();
        out += alphabet[r % L];
    }
    return out;
}

static void startSetupMode(bool wipe) {
    if (wipe) wipeProvisioningKeys(true);
    in_setup_mode = true;

    // Switching Wi-Fi modes while the async webserver is live can trigger lwIP asserts
    // if we fully turn Wi-Fi off. Keep the TCP/IP stack up; just move into AP mode.
    dnsServer.stop();
    WiFi.disconnect(false);
    // IMPORTANT: don't call `softAPdisconnect(true)` here (wifioff=true). That can tear down
    // lwIP internals while AsyncTCP tasks are still running, leading to `Invalid mbox` asserts.
    WiFi.softAPdisconnect(false);
    delay(100); // let the WiFi event loop settle
    // Keep STA alive (harmless) to avoid churn in the underlying netif/tcpip plumbing.
    WiFi.mode(WIFI_AP_STA);

    setup_ap_ssid = "ParkPal-Setup-" + randomAlphaNum(4);
    setup_ap_pass = randomAlphaNum(12);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress netM(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netM);
    WiFi.softAP(setup_ap_ssid.c_str(), setup_ap_pass.c_str());

    dnsServer.start(53, "*", apIP);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawText(BORDER_MARGIN, BORDER_MARGIN + 40, "PARKPAL SETUP", &FreeSansBold18pt7b, GxEPD_BLACK);
        int y = BORDER_MARGIN + 100;
        drawText(BORDER_MARGIN, y, "Wi-Fi:", &FreeSans12pt7b, GxEPD_BLACK);
        y += 30;
        drawText(BORDER_MARGIN, y, setup_ap_ssid, &FreeSansBold12pt7b, GxEPD_BLACK);
        y += 40;
        drawText(BORDER_MARGIN, y, "Password:", &FreeSans12pt7b, GxEPD_BLACK);
        y += 30;
        drawText(BORDER_MARGIN, y, setup_ap_pass, &FreeSansBold12pt7b, GxEPD_BLACK);
        y += 50;
        drawText(BORDER_MARGIN, y, "Open: http://192.168.4.1", &FreeSans12pt7b, GxEPD_BLACK);
    } while (display.nextPage());
}

void setup() {
    Serial.begin(115200);
    WiFi.onEvent(onWiFiEvent);
    pinMode(BOOT_PIN, INPUT_PULLUP);
    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
    display.init(115200, true, 2, false);
    display.setRotation(4);

    loadProvisioningKeys();
    Serial.println();
    Serial.println("=== ParkPal boot ===");
    Serial.printf("Provisioned: %s\n", isProvisioned() ? "yes" : "no");
    Serial.printf("Just provisioned: %s\n", just_provisioned ? "yes" : "no");
    if (WIFI_SSID.length()) Serial.printf("WiFi SSID: %s\n", WIFI_SSID.c_str());
    if (API_BASE_URL.length()) Serial.printf("API Base: %s\n", API_BASE_URL.c_str());
    if (!isProvisioned()) {
        startSetupMode(false);
        startWeb();
        return;
    }

    connectWiFi();
    Serial.printf("WiFi status after connect: %d\n", (int)WiFi.status());
    if (WiFi.status() != WL_CONNECTED && just_provisioned) {
        Serial.println("WiFi connect failed after provisioning; returning to setup mode.");
        startSetupMode(false);
        startWeb();
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
        initNTP();
    } else {
        Serial.println("WiFi not connected; skipping NTP sync.");
    }
    startWeb();
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    Serial.println("--- Current Local Time (startup default) ---");
    Serial.print(asctime(timeinfo));
    Serial.println("--------------------------------------------");
    migrateResolveIdsIfNeeded();
    if (MDNS.begin("parkpal")) Serial.println("mDNS started: http://parkpal.local/");
    IPAddress ip = WiFi.localIP();
    String ipStr = ip.toString();
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawText(BORDER_MARGIN, BORDER_MARGIN + 40, "ParkPal", &FreeSansBold18pt7b, GxEPD_BLACK);
        drawText(BORDER_MARGIN, BORDER_MARGIN + 80, WiFi.isConnected() ? "WiFi connected" : "WiFi offline", &FreeSans12pt7b, WiFi.isConnected() ? GxEPD_BLACK : GxEPD_RED);
        drawText(BORDER_MARGIN, BORDER_MARGIN + 110, "Open: parkpal.local", &FreeSans12pt7b, GxEPD_BLACK);
        drawText(BORDER_MARGIN, BORDER_MARGIN + 140, "IP: " + ipStr, &FreeSans12pt7b, GxEPD_BLACK);
    } while (display.nextPage());
}

void loop() {
    if (pending_restart && (int32_t)(millis() - restart_at_ms) >= 0) {
        Serial.println("Restarting now...");
        delay(50);
        ESP.restart();
    }
    if (in_setup_mode) {
        dnsServer.processNextRequest();
        delay(10);
        return;
    }

    // Factory reset gesture (BOOT long press while running)
    const bool bootPressed = digitalRead(BOOT_PIN) == LOW;
    const unsigned long nowMs = millis();
    if (bootPressed) {
        if (boot_press_start_ms == 0) boot_press_start_ms = nowMs;
        if ((uint32_t)(nowMs - boot_press_start_ms) >= FACTORY_RESET_HOLD_MS) {
            renderMessage("Factory Reset...", MSG_FONT);
            startSetupMode(true);
            boot_press_start_ms = 0;
            return;
        }
    } else {
        boot_press_start_ms = 0;
    }

    // Wi-Fi fallback to AP after sustained disconnect
    if (WiFi.status() != WL_CONNECTED) {
        if (wifi_disconnected_since_ms == 0) wifi_disconnected_since_ms = nowMs;
        if ((uint32_t)(nowMs - wifi_disconnected_since_ms) >= WIFI_AP_FALLBACK_AFTER_MS) {
            renderMessage("WiFi Failed - Setup", MSG_FONT);
            startSetupMode(false);
            wifi_disconnected_since_ms = 0;
            return;
        }
    } else {
        wifi_disconnected_since_ms = 0;
    }

    // Config save should trigger an immediate refresh, even if the caller doesn't hit /api/refresh.
    if (config_changed) {
        config_changed = false;
        parks_lastFrameKey = "";
        countdowns_lastFrameKey = "";
        refresh_now = true;
    }

    // Opportunistic reconnect in the background even between refreshes.
    if (WiFi.status() != WL_CONNECTED) ensureWiFiConnected(0);

    if (refresh_now || millis() - lastTick >= REFRESH_MS || lastTick == 0) {
        lastTick = millis();
        refresh_now = false;
        RuntimeConfig RC;
        if (!parseConfig(RC)) {
            renderMessage("Config Error", MSG_FONT);
            return;
        }
        static String lastMode = "";
        if (RC.mode != lastMode) {
            parks_lastFrameKey = "";
            countdowns_lastFrameKey = "";
            lastMode = RC.mode;
            countdownCycleIndex = 0;
            countdownRefreshCounter = 0;
        }
        if (RC.mode == "parks") {
            if (RC.parks_n == 0) {
                renderMessage("No Parks Selected", MSG_FONT);
                return;
            }
            int idx = parkIndex % RC.parks_n;
            parkIndex = (parkIndex + 1) % RC.parks_n;
            const int parkId = RC.parks[idx];
            String parkName = parkNameForId(parkId);
            int ids[6];
            String labels[6];
            String legacy[6];
            for (int i = 0; i < 6; i++) {
                ids[i] = RC.rideIds[idx][i];
                labels[i] = RC.rideLabels[idx][i];
                legacy[i] = RC.legacyNames[idx][i];
            }
            DynamicJsonDocument doc(16 * 1024);
            const bool wifiOk = ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS);
            bool ok = wifiOk && fetchSummaryForPark(parkId, RC.metric, ids, doc);
            if (!ok && wifiOk) {
                api_fail_streak++;
                if (api_fail_streak >= API_FAIL_STREAK_WIFI_RESET) {
                    api_fail_streak = 0;
                    WiFi.disconnect(false);
                    delay(250);
                    connectWiFi();
                    kickNTP();
                    if (WiFi.status() == WL_CONNECTED) {
                        ok = fetchSummaryForPark(parkId, RC.metric, ids, doc);
                    }
                }
            } else if (ok) {
                api_fail_streak = 0;
            }

            if (ok) {
                String tripName = RC.trip_name;
                if (!tripName.length()) tripName = inferTripNameFromParks(RC.resort, RC.parks, RC.parks_n);
                renderParks(doc, ids, labels, parkName, RC.metric, RC.trip_enabled, RC.trip_date, tripName, legacy, RC.parks_tz.c_str());
            } else {
                if (wifiOk) {
                    // Retry sooner than the normal refresh interval.
                    lastTick = millis() - (REFRESH_MS - API_ERROR_RETRY_MS);
                    renderMessage(last_http_code > 0 ? ("API HTTP " + String(last_http_code)) : "API Error", MSG_FONT);
                } else {
                    String msg = "WiFi offline";
                    if (last_wifi_disconnect_reason) {
                        msg += " (";
                        msg += wifiReasonToStr(last_wifi_disconnect_reason);
                        msg += ")";
                    }
                    renderMessage(msg, MSG_FONT);
                }
            }
        } else { // Countdown mode
            CountdownItem activeItem;
            bool itemAvailable = false;
            if (RC.countdowns.empty()) {
                renderMessage("No Countdowns Configured", MSG_FONT);
                return;
            }
            if (RC.countdownSettings.show_mode == "single") {
                String want = RC.countdownSettings.primary_id;
                if (want.length() == 0) want = RC.countdowns[0].id;
                for (const auto& item : RC.countdowns) {
                    if (item.id == want) {
                        activeItem = item;
                        itemAvailable = true;
                        break;
                    }
                }
                if (!itemAvailable) activeItem = RC.countdowns[0];
            } else { // cycle
                std::vector<CountdownItem> cycleItems;
                for (const auto& item : RC.countdowns)
                    if (item.include_in_cycle) cycleItems.push_back(item);
                if (cycleItems.empty()) {
                    renderMessage("No Countdowns in Cycle", MSG_FONT);
                    return;
                }
                if (++countdownRefreshCounter >= RC.countdownSettings.cycle_every_n_refreshes) {
                    countdownRefreshCounter = 0;
                    countdownCycleIndex = (countdownCycleIndex + 1) % cycleItems.size();
                }
                activeItem = cycleItems[cycleItems.size() > 0 ? countdownCycleIndex % cycleItems.size() : 0];
            }
            int days, turnsAge;
            computeDaysToEvent(activeItem, RC.countdowns_tz.c_str(), days, turnsAge);
            if (days == -2) { // NTP not ready
                renderMessage("Syncing Time...", MSG_FONT);
            } else {
                renderCountdowns(activeItem, days, turnsAge);
            }
        }
    }

    // Yield to keep WiFi/webserver healthy.
    delay(10);
}

// -------------------- Legacy Migration Logic --------------------
void migrateResolveIdsIfNeeded() {
    String s = loadConfigJson();
    DynamicJsonDocument dj(24 * 1024);
    if (deserializeJson(dj, s)) return;
    if (dj.containsKey("parks_tz")) return; // new schema present
    JsonArray pe = dj["parks_enabled"].as<JsonArray>();
    if (pe.isNull()) return;
    bool anyChanged = false;
    for (JsonVariant v : pe) {
        int pid = (int)v;
        if (pid == 6 || pid == 5 || pid == 7 || pid == 8) {
            if (resolveParkSlotsToIds(pid, dj)) anyChanged = true;
        }
    }
    if (anyChanged) {
        String out;
        serializeJson(dj, out);
        saveConfigJson(out);
    }
}

bool resolveParkSlotsToIds(int parkId, JsonDocument& cfgDoc) {
    JsonArray ids = cfgDoc["rides_by_park_ids"][String(parkId)].to<JsonArray>();
    JsonArray labs = cfgDoc["rides_by_park_labels"][String(parkId)].to<JsonArray>();
    JsonArray leg = cfgDoc["rides_by_park"][String(parkId)].as<JsonArray>();
    while (ids.size() < 6) ids.add(0);
    while (labs.size() < 6) labs.add("");
    DynamicJsonDocument doc(24 * 1024);
    if (API_BASE_URL.length() == 0) return false;
    String url = apiUrl("/v1/rides") + "?park=" + String(parkId);
    if (!httpGetJson(url, doc)) return false;
    JsonArray canon = doc["rides"].as<JsonArray>();
    if (canon.isNull()) return false;
    bool changed = false;
    for (int i = 0; i < 6; i++) {
        int curId = (int)ids[i];
        String label = String(labs[i] | "");
        String legacy = leg.isNull() ? String("") : String(leg[i] | "");
        if (curId > 0) continue;
        String want = label.length() ? label : legacy;
        if (want.length() == 0) continue;
        String wantN = normalize(want);
        for (JsonVariant r : canon) {
            String nm = String(r["name"] | "");
            if (normalize(nm) == wantN) {
                ids[i] = (int)r["id"];
                labs[i] = nm;
                changed = true;
                break;
            }
        }
    }
    return changed;
}
