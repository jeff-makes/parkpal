# ParkPal

A self-contained Disney park dashboard built on an ESP32 and a 7.5" tri-color e-ink display. It sits on your desk (or kitchen counter, or nightstand) and shows:

- **Wait times** for your favorite rides, updated every 30 minutes
- **Live weather** for whichever park you're tracking
- **Countdown screens** — trip countdowns, birthdays, holidays, with festive icons

Supports **Walt Disney World**, **Disneyland Resort**, and **Tokyo Disney Resort**.

<!-- TODO: hero photo of the finished device here -->
<!-- TODO: close-up of the e-ink screen showing wait times -->
<!-- TODO: screenshot of the web config UI (desktop + mobile) -->
<!-- TODO: short video/GIF of a screen refresh -->

## No Shared API — You Host Your Own

ParkPal does **not** phone home to someone else's server.

Instead, you deploy a tiny **Cloudflare Worker** (free tier is plenty) that acts like your personal “backend”:
- It fetches ride wait times from Queue‑Times and weather from OpenWeather.
- It **caches** results so you don’t hammer upstream APIs.
- It keeps your OpenWeather API key **out of the firmware** (the ESP32 never needs to store it).

Your ParkPal device talks only to *your* Worker URL.

## How Hard Is This?

| | |
|---|---|
| **Soldering** | None. The driver board plugs into the panel via a flat cable. |
| **Parts** | 2 things to buy (board + panel), plus a USB cable you already own. |
| **Software** | Flash one `.ino`, run two terminal commands for the Worker. |
| **Time to build** | About an hour if everything goes smoothly. Most of that is waiting for Arduino to compile. |

## What You Need

| Part | Notes | Approx. Price |
|---|---|---|
| ESP32 e-paper driver board (SPI) | Waveshare universal board or similar | ~$20 |
| 7.5" tri-color e-ink panel (880x528) | Black/white/red, SPI interface | ~$25 |
| USB-C cable + power supply | Any phone charger works | — |
| Frame | IKEA RÖDALM 13x18 cm (5x7 in) | — |

**Links (what I used):**
- E‑ink panel (7.5" tri‑color, 880×528): https://www.aliexpress.com/item/1005004369892606.html
- ESP32 e‑paper driver board: https://www.aliexpress.com/item/1005007030138070.html
- Frame: IKEA RÖDALM 13×18 cm (5×7 in)

The driver board and panel connect via a ribbon cable — no soldering, no breadboard.

## Quick Start

1. **Flash** the firmware (`parkpal.ino`) to your ESP32.
2. **Deploy** the backend Worker to your Cloudflare account.
3. **Power on** ParkPal — it starts a setup Wi-Fi network.
4. **Join the setup Wi-Fi** and enter your home Wi-Fi credentials + Worker URL.
5. **Open** `http://parkpal.local/` from your phone/laptop and pick your parks + rides.

The rest of this README walks through each step.

## 1. Deploy the Backend (Cloudflare Worker)

Each builder deploys their own Worker. It takes about 2 minutes.

**What is Wrangler?** Wrangler is Cloudflare’s command-line tool that uploads this repo’s `worker.js` + `parks.json` to your Cloudflare account and lets you set secrets (like your OpenWeather API key) safely. You *can* paste code into the Cloudflare dashboard editor, but Wrangler is the easiest/least-error-prone way to deploy this project because it’s more than one file.

**Prerequisites:** [Node.js](https://nodejs.org/) installed.

```bash
# Install the Cloudflare CLI
npm install -g wrangler

# Log in to your (free) Cloudflare account
wrangler login

# Deploy from this repo root
wrangler deploy
```

Wrangler will print your Worker URL — something like `https://parkpal-api.YOUR-SUBDOMAIN.workers.dev`. That URL is now live on Cloudflare’s servers. Copy it; you'll need it during device setup.

**Set your OpenWeather API key:**

ParkPal uses [OpenWeather](https://openweathermap.org/api) for weather data. Sign up for a free account and grab an API key, then:

```bash
wrangler secret put OWM_API_KEY
# paste your key when prompted
```

**Local development:**

For local testing with `wrangler dev`, put secrets in a `.dev.vars` file (already in `.gitignore`):

```
OWM_API_KEY=your_key_here
```

Never put `OWM_API_KEY` in `wrangler.toml` — that file is committed.

## 2. Flash the Firmware (Arduino IDE)

**Board support:**

- Open Arduino IDE
- Go to **Boards Manager** and install **"esp32" by Espressif Systems**
- Select your ESP32 board (e.g., "ESP32 Dev Module")

**Libraries** (install via Arduino Library Manager):

| Library | Notes |
|---|---|
| `GxEPD2` | E-ink display driver |
| `ArduinoJson` | Version 6.x |
| `ESPAsyncWebServer` | Async HTTP server |
| `AsyncTCP` | Required by ESPAsyncWebServer |

Open `parkpal.ino` and hit Upload.

> **"Sketch too big"?** In Arduino IDE, go to Tools > Partition Scheme and pick one with a larger app slot (e.g., "Huge APP").

## 3. First Boot / Setup Mode

On first power-up (or after a factory reset), ParkPal can't connect to Wi-Fi yet, so it starts its own access point and shows the credentials on the e-ink screen:

1. **Look at the screen** — it displays a Wi-Fi name (like `ParkPal-Setup-Ab3x`) and the setup password.
2. **Connect** to that Wi-Fi from your phone or laptop.
3. **Open** `http://192.168.4.1/` — a setup page appears.
4. **Enter** your home Wi-Fi SSID, password, and your Worker URL.
   - Paste the base URL only (e.g., `https://parkpal-api.my-sub.workers.dev`). Do **not** include `/v1`.
5. **Save** — ParkPal reboots and connects to your Wi-Fi.

After it connects, open `http://parkpal.local/` to configure which parks and rides to display.

**Setup network password:** `parkpal1234`

**Factory reset:** Hold the **BOOT** button for ~8 seconds while ParkPal is running. It wipes Wi-Fi credentials and config, then re-enters setup mode.

> Heads up: holding BOOT while pressing the **EN** (reset) button puts the ESP32 into USB flashing mode instead. For factory reset, just hold BOOT by itself while the device is already running.

If ParkPal loses Wi-Fi for more than 5 minutes, it automatically falls back to setup mode so you can re-enter credentials without needing to factory reset.

## Troubleshooting

**Wi-Fi requirements / compatibility**
- **2.4 GHz only.** ESP32 can't join 5 GHz networks.
- **WPA2-Personal (PSK) recommended.** WPA3-only or some “WPA2/WPA3 + PMF required” configs can break ESP32 clients.
- **Captive portal / hotel Wi‑Fi won’t work** (no browser login flow on the device).
- **Mesh Wi‑Fi:** if you have multiple access points broadcasting the same SSID, ParkPal locks to the strongest AP (BSSID) and will try another AP if the handshake fails.

**`parkpal.local` doesn't work (especially on Android)**
mDNS support varies by device. Android in particular often doesn't resolve `.local` names. Use the device's IP address instead — it's printed on the e-ink screen at startup and on the Serial monitor.

**Screen says "API Error" or "API HTTP 4xx/5xx"**
- Double-check your Worker URL in setup. It should be the base URL without `/v1`.
- Make sure you ran `wrangler secret put OWM_API_KEY` and the key is valid.
- Try hitting `https://your-worker-url/v1/health` in a browser — you should see `{"ok":true}`.

**Ride data shows "Closed" for everything**
The upstream ride data source (Queue-Times) may be down, or the park may actually be closed. Check `https://your-worker-url/v1/status` for cache health and error details.

**Weather shows 0 degrees**
Your OpenWeather API key is probably missing or invalid. Run `wrangler secret put OWM_API_KEY` again.

**"Sketch too big" during upload**
Change your partition scheme in Arduino IDE: Tools > Partition Scheme > "Huge APP (3MB No OTA / 1MB SPIFFS)" or similar.

## Pin Mapping

If you're using a different ESP32 board, you may need to adjust the SPI pins in `parkpal.ino`:

| Signal | Default GPIO |
|---|---|
| `EPD_CS` | 15 |
| `EPD_DC` | 27 |
| `EPD_RST` | 26 |
| `EPD_BUSY` | 25 |
| `EPD_SCK` | 13 |
| `EPD_MOSI` | 14 |

The Waveshare ESP32 e-paper driver board uses these by default — no changes needed if you're using that board.

## Repo Layout

```
parkpal/
├── parkpal.ino      # ESP32 firmware (Arduino)
├── html.h           # Web config UI (served by the ESP32)
├── setup_html.h     # Captive portal setup page
├── WeatherIcons.h   # Weather icons (1-bit bitmaps, MIT)
├── worker.js        # Cloudflare Worker (your self-hosted backend)
├── parks.json       # Park registry (IDs, coordinates, timezones)
├── wrangler.toml    # Wrangler config for the Worker
└── LICENSE          # MIT
```

## Supported Parks

| Destination | Parks |
|---|---|
| Walt Disney World (Florida) | Magic Kingdom, EPCOT, Hollywood Studios, Animal Kingdom |
| Disneyland Resort (California) | Disneyland, Disney California Adventure |
| Tokyo Disney Resort (Japan) | Tokyo Disneyland, Tokyo DisneySea |

Adding more parks is just a `parks.json` edit — PRs welcome if you verify the Queue-Times park IDs.

## License

MIT. See [LICENSE](LICENSE).

## Disclaimer

Disney, Walt Disney World, Disneyland, and related names are trademarks of The Walt Disney Company. This project is not affiliated with or endorsed by Disney.

ParkPal uses third-party data sources ([Queue-Times](https://queue-times.com/) for ride wait times, [OpenWeather](https://openweathermap.org/) for weather). Their availability and terms of service apply.

**Powered by [Queue-Times.com](https://queue-times.com/).** Seriously: huge kudos to them for maintaining one of the largest public datasets of theme-park wait times in the world.

Weather icons are based on Bas Milius’ [weather-icons](https://github.com/basmilius/weather-icons) (MIT). See `third_party/weather-icons.LICENSE.txt`.
