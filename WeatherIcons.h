// WeatherIcons.h — Weather icons drawn with Adafruit GFX primitives.
// No bitmap arrays; shapes are composed from circles, rectangles, lines,
// and triangles. Designed for monochrome e-ink at small sizes (48x32).
//
// Usage: drawWeatherIcon(display, x, y, owmCode, desc, GxEPD_BLACK, isNight, GxEPD_WHITE);

#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// Bounding box for all weather icons (used by caller for layout).
static const int16_t WEATHER_ICON_W = 48;
static const int16_t WEATHER_ICON_H = 32;

// ---- Internal drawing helpers ----

// Cloud shape: cx = horizontal center, baseY = flat bottom edge, s = scale.
// Width ≈ 4*s, height ≈ 1.5*s.
static void _wCloud(Adafruit_GFX& g, int16_t cx, int16_t baseY,
                    int16_t s, uint16_t c) {
    int16_t r1 = s;                    // center bump radius
    int16_t r2 = (s * 3 + 2) / 4;     // side bump radius (≈ 0.75*s)
    int16_t top = baseY - s;           // top of flat body
    g.fillCircle(cx,              top - s / 2, r1, c);   // center (tallest)
    g.fillCircle(cx - s * 5 / 4, top,         r2, c);   // left
    g.fillCircle(cx + s * 5 / 4, top,         r2, c);   // right
    g.fillRect(cx - s * 2, top, s * 4, s, c);            // flat body
}

// Sun: filled circle + 8 rays at 45-degree intervals.
// Cardinal rays are 3 px thick; diagonal rays are 2 px thick so they
// read clearly on monochrome e-ink.
static void _wSun(Adafruit_GFX& g, int16_t cx, int16_t cy,
                  int16_t r, int16_t ray, uint16_t c) {
    g.fillCircle(cx, cy, r, c);
    int16_t in  = r + 2;
    int16_t out = r + 2 + ray;
    int16_t dI  = (in  * 707 + 500) / 1000;   // cos(45°) ≈ 0.707
    int16_t dO  = (out * 707 + 500) / 1000;
    // Cardinal rays — 3 px thick
    for (int16_t d = -1; d <= 1; d++) {
        g.drawLine(cx + d, cy - in, cx + d, cy - out, c);  // N
        g.drawLine(cx + in, cy + d, cx + out, cy + d, c);   // E
        g.drawLine(cx + d, cy + in, cx + d, cy + out, c);   // S
        g.drawLine(cx - in, cy + d, cx - out, cy + d, c);   // W
    }
    // Diagonal rays — 2 px thick (symmetric offsets for visual balance)
    g.drawLine(cx + dI,     cy - dI, cx + dO,     cy - dO, c);  // NE
    g.drawLine(cx + dI + 1, cy - dI, cx + dO + 1, cy - dO, c);
    g.drawLine(cx + dI, cy + dI,     cx + dO, cy + dO,     c);  // SE
    g.drawLine(cx + dI + 1, cy + dI, cx + dO + 1, cy + dO, c);
    g.drawLine(cx - dI,     cy + dI, cx - dO,     cy + dO, c);  // SW
    g.drawLine(cx - dI - 1, cy + dI, cx - dO - 1, cy + dO, c);
    g.drawLine(cx - dI,     cy - dI, cx - dO,     cy - dO, c);  // NW
    g.drawLine(cx - dI - 1, cy - dI, cx - dO - 1, cy - dO, c);
}

// Rain: two staggered rows of diagonal streaks (2 px thick).
static void _wRain(Adafruit_GFX& g, int16_t left, int16_t top,
                   int16_t w, uint16_t c) {
    int16_t sp = w / 4;
    int16_t sx = left + sp / 2;
    for (int i = 0; i < 3; i++) {                          // row 1: 3 drops
        int16_t px = sx + i * sp;
        g.drawLine(px, top, px + 2, top + 5, c);
        g.drawLine(px + 1, top, px + 3, top + 5, c);
    }
    for (int i = 0; i < 2; i++) {                          // row 2: 2 drops
        int16_t px = sx + sp / 2 + i * sp;
        g.drawLine(px, top + 5, px + 2, top + 10, c);
        g.drawLine(px + 1, top + 5, px + 3, top + 10, c);
    }
}

// Lightning bolt: two filled triangles forming a zigzag.
static void _wBolt(Adafruit_GFX& g, int16_t cx, int16_t top,
                   int16_t bot, uint16_t c) {
    int16_t mid = (top + bot) / 2;
    g.fillTriangle(cx - 3, top, cx + 4, top,     cx + 1, mid + 1, c);
    g.fillTriangle(cx - 1, mid, cx + 5, mid - 1, cx - 4, bot,     c);
}

// Snow: two staggered rows of small filled circles.
static void _wSnow(Adafruit_GFX& g, int16_t left, int16_t top,
                   int16_t w, uint16_t c) {
    int16_t sp = w / 4;
    int16_t sx = left + sp / 2;
    for (int i = 0; i < 3; i++)
        g.fillCircle(sx + i * sp, top, 2, c);
    for (int i = 0; i < 2; i++)
        g.fillCircle(sx + sp / 2 + i * sp, top + 7, 2, c);
}

// Moon crescent: filled circle with a same-radius circle carved out,
// shifted left to leave a right-facing crescent.
// fg = foreground (drawn), bg = background (eraser).
static void _wMoon(Adafruit_GFX& g, int16_t cx, int16_t cy,
                   int16_t r, uint16_t fg, uint16_t bg) {
    g.fillCircle(cx, cy, r, fg);
    // Offset the cutout left by ~40 % of radius; same radius → uniform crescent.
    int16_t off = (r * 2 + 2) / 5;           // ≈ 0.4 * r, rounded
    g.fillCircle(cx - off, cy, r, bg);
}

// Tiny star: 5-pixel plus shape (visible at small sizes on e-ink).
static void _wStar(Adafruit_GFX& g, int16_t cx, int16_t cy, uint16_t c) {
    g.drawPixel(cx,     cy,     c);
    g.drawPixel(cx - 1, cy,     c);
    g.drawPixel(cx + 1, cy,     c);
    g.drawPixel(cx,     cy - 1, c);
    g.drawPixel(cx,     cy + 1, c);
}

// ---- Public icon functions ----
// Each draws within a WEATHER_ICON_W x WEATHER_ICON_H box at (x, y).

static void drawIconSunny(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c) {
    _wSun(g, x + 24, y + 16, 8, 5, c);       // r=8 disc, 5 px rays
}

static void drawIconClearNight(Adafruit_GFX& g, int16_t x, int16_t y,
                               uint16_t c, uint16_t bg) {
    _wMoon(g, x + 24, y + 16, 10, c, bg);    // crescent, ~4 px wide
    _wStar(g, x +  8, y +  6, c);
    _wStar(g, x + 40, y + 10, c);
    _wStar(g, x + 10, y + 26, c);
}

static void drawIconPartlyCloudy(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c) {
    _wSun(g, x + 34, y + 10, 5, 4, c);       // sun peeks upper-right
    _wCloud(g, x + 20, y + 26, 7, c);         // cloud overlaps lower-left
}

static void drawIconPartlyCloudyNight(Adafruit_GFX& g, int16_t x, int16_t y,
                                      uint16_t c, uint16_t bg) {
    _wMoon(g, x + 35, y + 9, 7, c, bg);      // moon peeks upper-right
    _wCloud(g, x + 20, y + 26, 7, c);         // cloud overlaps lower-left
}

static void drawIconCloudy(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c) {
    _wCloud(g, x + 24, y + 27, 8, c);
}

static void drawIconRain(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c) {
    _wCloud(g, x + 24, y + 18, 7, c);
    _wRain(g, x + 6, y + 21, 36, c);
}

static void drawIconThunderstorm(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c) {
    _wCloud(g, x + 24, y + 18, 7, c);
    _wBolt(g, x + 24, y + 20, y + 31, c);
}

static void drawIconSnow(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c) {
    _wCloud(g, x + 24, y + 18, 7, c);
    _wSnow(g, x + 6, y + 22, 36, c);    // shifted up 1 px to keep in bounds
}

// ---- Dispatcher: maps OWM condition code to icon ----
// isNight  – true when current time is before sunrise or after sunset.
// bg       – background color (needed for moon crescent cutout).

static void drawWeatherIcon(Adafruit_GFX& g, int16_t x, int16_t y,
                            int code, const String& desc, uint16_t color,
                            bool isNight = false, uint16_t bg = 0xFFFF) {
    // Prefer OpenWeather condition code (most reliable).
    if (code >= 200 && code <= 232) { drawIconThunderstorm(g, x, y, color); return; }
    if (code >= 300 && code <= 531) { drawIconRain(g, x, y, color); return; }
    if (code >= 600 && code <= 622) { drawIconSnow(g, x, y, color); return; }
    if (code >= 701 && code <= 781) { drawIconCloudy(g, x, y, color); return; }
    if (code == 800) {
        if (isNight) drawIconClearNight(g, x, y, color, bg);
        else         drawIconSunny(g, x, y, color);
        return;
    }
    if (code == 801 || code == 802) {
        if (isNight) drawIconPartlyCloudyNight(g, x, y, color, bg);
        else         drawIconPartlyCloudy(g, x, y, color);
        return;
    }
    if (code >= 803 && code <= 804) { drawIconCloudy(g, x, y, color); return; }

    // Fallback: match by description text (keeps older Workers working).
    String d = desc;
    d.toLowerCase();
    if (d.indexOf("thunder") >= 0 || d.indexOf("storm") >= 0)
        { drawIconThunderstorm(g, x, y, color); return; }
    if (d.indexOf("rain") >= 0 || d.indexOf("drizzle") >= 0)
        { drawIconRain(g, x, y, color); return; }
    if (d.indexOf("snow") >= 0)
        { drawIconSnow(g, x, y, color); return; }
    if (d.indexOf("clear") >= 0) {
        if (isNight) drawIconClearNight(g, x, y, color, bg);
        else         drawIconSunny(g, x, y, color);
        return;
    }
    if (d.indexOf("few cloud") >= 0 || d.indexOf("scattered") >= 0) {
        if (isNight) drawIconPartlyCloudyNight(g, x, y, color, bg);
        else         drawIconPartlyCloudy(g, x, y, color);
        return;
    }
    if (d.indexOf("cloud") >= 0)
        { drawIconCloudy(g, x, y, color); return; }
    if (d.indexOf("mist") >= 0 || d.indexOf("fog") >= 0 || d.indexOf("haze") >= 0)
        { drawIconCloudy(g, x, y, color); return; }

    // Unknown: small dash
    g.fillRect(x + 20, y + 14, 8, 3, color);
}
