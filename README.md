# ParkPal

An ESP32 + 7.5" tri‑color e‑ink display that shows:
- Disney park wait times (your favorite rides)
- Weather for the selected park
- Countdown screens (trip + holidays/birthdays/etc.)

This repo is meant to be **builder-friendly**:
- No hardcoded Wi‑Fi secrets
- No shared/public backend API
- You deploy your own Cloudflare Worker (free tier works great)
- First boot starts a setup Wi‑Fi + captive portal

## Photos / Demos (TODO)

- TODO: add finished device photo
- TODO: add close-up e‑ink photo
- TODO: add web UI screenshot (desktop + mobile)

## What You Need (BOM)

**Required:**
- ESP32 e‑paper driver board (SPI)
- 7.5" tri‑color e‑ink panel (880×528)
- USB cable + power supply

**Links (TODO):**
- Driver board: TODO
- Panel: TODO

## Quick Start

1. Flash the firmware (`parkpal.ino`) to your ESP32.
2. Deploy the backend Worker (`worker.js`) to your own Cloudflare account.
3. Set your OpenWeather key (`OWM_API_KEY`) on the Worker.
4. Power on ParkPal → join the setup Wi‑Fi → enter your Wi‑Fi + Worker URL.
5. Open `http://parkpal.local/` → pick parks + rides.

## Deploy the Backend (Cloudflare Worker)

ParkPal does **not** use a shared hosted API. Each builder deploys their own Worker.

1. Install Wrangler: `npm install -g wrangler`
2. Login: `wrangler login`
3. Deploy from this repo root: `wrangler deploy`
4. Set your OpenWeather key: `wrangler secret put OWM_API_KEY`
5. Copy the printed Worker URL — you’ll paste this into ParkPal setup.

Local dev note:
- Use `.dev.vars` for local secrets (already ignored by `.gitignore`).
- Don’t put `OWM_API_KEY` in `wrangler.toml`.

## First Boot / Setup Mode

On first boot (or after factory reset), ParkPal starts a setup Wi‑Fi network and captive portal:
- SSID + password are shown on the e‑ink screen
- Connect and open `http://192.168.4.1/`
- Enter your home Wi‑Fi SSID/password + your Worker URL
- ParkPal reboots and connects

Reset back to setup:
- Hold the `BOOT` button for ~8 seconds while it’s running (factory reset)
  - Note: holding `BOOT` while pressing reset (`EN`) may put the ESP32 into flashing mode instead.

## Firmware Build (Arduino IDE)

You’ll need:
- Arduino IDE
- ESP32 board support by Espressif (“esp32” in Boards Manager)

Libraries (Arduino Library Manager):
- `GxEPD2`
- `ArduinoJson` (v6)
- `ESPAsyncWebServer`
- `AsyncTCP`

If you hit “Sketch too big”, pick a partition scheme with a larger app slot.

## Pin Notes (Only If You Swap Boards)

Defaults in `parkpal.ino`:
- `EPD_CS = 15`
- `EPD_DC = 27`
- `EPD_RST = 26`
- `EPD_BUSY = 25`
- `EPD_SCK = 13`
- `EPD_MOSI = 14`

## Repo Layout

```
parkpal/
├── parkpal.ino      # ESP32 firmware
├── html.h           # Web UI served by the ESP32
├── setup_html.h     # Captive portal setup page
├── worker.js        # Cloudflare Worker backend (self-hosted)
├── parks.json       # Destination/park registry
├── wrangler.toml    # Worker config
├── WeatherIcons.h   # Bitmap icons
├── IMPLEMENTATION.md
└── LICENSE
```

## Disclaimer

- Disney is a trademark of The Walt Disney Company.
- Uses third-party data sources (Queue-Times, OpenWeather); their uptime/terms apply.

