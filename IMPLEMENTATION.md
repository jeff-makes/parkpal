# ParkPal Implementation Plan (Open Source + Self-Hosted API)

This doc is a concrete, buildable plan for turning ParkPal into a public open source project with a **self-hosted-by-default** backend (each builder deploys their own Cloudflare Worker).

## Progress tracker (for future sessions)

Keep this section up to date as work lands so you can resume quickly in a new session.

**Done (so far):**
- [x] Worker: remove deferred features (cron `scheduled()`, warm-cache/prefetch, replay/history branches). (commit: `cd087ad`)
- [x] Worker: add `GET /v1/destinations` (temporary implementation backed by in-file `REGIONS`, Orlando+Tokyo). (commit: `a5c404e`)
- [x] Worker: add `parks.json` + `wrangler.toml`; switch `/v1/destinations` to be sourced from `parks.json` (Orlando+Tokyo). (commit: `106fd74`)
- [x] Worker: make `/v1/regions` a deprecated alias returning the same payload as `/v1/destinations`. (commit: `018ab64`)
- [x] Worker: refactor `GET /v1/rides?park=<id>` to per-park (sourced from `parks.json`) with a 24h Cache API entry per park. (commit: `8de1617`)
- [x] Worker: refactor `POST /v1/summary` to per-park (sourced from `parks.json`) with a 30m Cache API entry per park+units and park-specific weather coords. (commit: `2b9b600`)
- [x] Worker: enforce v1 favorite rules (`favorite_ride_ids` max 6; empty favorites → `rides: []`). (commit: `68b3833`)
- [x] Worker: update `/v1/status` to report per-park summary cache health (park+units keys). (commit: `6589dbc`)
- [x] Worker: remove dead region-based code paths (`REGIONS`, warm-cache/replay leftovers). (commit: `5381914`)
- [x] Firmware: Phase 0 scrub (remove hardcoded secrets/personal defaults; remove replay UI/logic). (commit: `32461c5`)
- [x] Firmware: sync firmware + UI to Worker v1 response shapes (`/v1/rides` and `/v1/summary`). (commit: `8f73ad9`)
- [x] Firmware: add AP captive portal provisioning (Wi‑Fi + `api_base_url`) + BOOT long-press factory reset. (commit: `aef7d02`)
- [x] Repo: add MIT `LICENSE`, `.gitignore`, and a self-hosted README skeleton. (commit: `a962fb2`)
- [x] Destinations: add California (Disneyland Resort) to `parks.json` and UI resort list. (commit: `d86b0bb`)
- [x] Docs: make this plan internally consistent + executable. (commit: `df30b90`)

**Next (recommended order):**
- [ ] Firmware: polish provisioning UX (clearer “needs setup” screen; add “enter setup mode” button in normal UI if desired).
- [ ] README: rewrite for the self-hosted Worker flow + add real BOM links + real photos.

## Context (why we’re doing this)

ParkPal is currently a personal project. The immediate goal is **interest validation**, not “product launch”:

- Make a public GitHub repo **now** (so you can link it immediately).
- Post a few great photos/videos on relevant subreddits and socials **and include the repo link** (or share it in comments when asked).

This plan intentionally prioritizes:
- **Simplicity for builders** (clear steps, minimal cloud complexity).
- **Low operational burden** for you (no hosted/shared backend, no tokens, no billing, no monitoring).
- **Documentation quality** (an “amazing README” that makes the project feel real and approachable).

## Release requirements (v1 public repo)

These are the requirements for a Reddit-ready release:

1. **A great README**
   - Clear “what it is” + what’s on the screen (parks + countdowns).
   - Photos of the physical device + at least one close-up of the e‑ink output.
   - Screenshot of the web UI (desktop + mobile if possible).
   - A short demo video/GIF (optional but strongly recommended).
2. **Build guide + BOM**
   - Bill of materials with links, approximate cost ranges, and known-good parts.
   - Wiring/pinout:
     - If using an integrated driver board + panel with a ribbon cable, document that it’s “plug-in” (no soldering).
     - Include the exact AliExpress links you used and a couple photos of the cable/connector.
     - Still list firmware pin assumptions for completeness (in case someone swaps boards).
3. **Self-host backend guide**
   - Cloudflare Worker setup steps (Wrangler or dashboard).
   - How to create and set `OWM_API_KEY`.
   - How to get Queue-Times data (no key; explain caveats).
4. **First-boot setup UX**
   - Device starts in AP provisioning if Wi‑Fi/backend URL aren’t configured.
   - Clear reset/back-to-setup mechanism using the device buttons.
5. **Basic troubleshooting**
   - Common failure modes: Wi‑Fi, NTP/time sync, Worker URL wrong, OWM key missing, upstream down.
6. **Repo hygiene**
   - No hardcoded personal secrets or personal default data.
   - License + attribution + disclaimer about third-party services/trademarks.

## Recommended (nice-to-have) for the README

- “Time to build” estimate (e.g., 1–3 hours).
- “Skill level” estimate (beginner-friendly with some soldering vs plug-and-play).
- Known limitations (tri-color refresh time, ghosting, update interval).
- Safety note: the provisioning AP is WPA2-protected; still do setup in a reasonably safe environment.

## “Reddit + GitHub” release moment (how to ship v1)

For v1, the Reddit post and the GitHub release are the *same moment*:
- Publish the public repo first (clean history, no secrets).
- Post to Reddit with photos + a short demo + link to the repo.
- Expect a wave of “how do I build it?” questions; the README is your support system.

## Minimum viable scope (to avoid getting stuck)

To keep this launch achievable, it’s OK to start with the destinations already proven in code:
- **Orlando (WDW)** + **Tokyo (TDR)** + **California (Disneyland Resort)** for launch
- Add Paris + Universal parks later (post-launch) if interest is validated

The README should be honest about what’s supported “today” vs “roadmap”.

## Explicitly deferred (v1)

These are intentionally **out of scope** for the first public release:

- Universal parks (Orlando / Hollywood / Japan)
- Disneyland Paris
- ParkPal-hosted/shared public API (tokens, rate limiting, monitoring, abuse handling)
- OTA updates (HTTPS OTA + rollback + signing)
- Replay mode / “time shift” history buffers
- Worker cron warm-cache / scheduled prefetch
- “All Disney parks worldwide” and other expansions

## Guiding decisions

- **Scope:** parks-only (no general travel dashboard in v1).
- **Launch destinations (v1):** Orlando (WDW), California (Disneyland Resort), Tokyo (TDR).
- **Backend:** self-hosted Worker per user (no public ParkPal API in v1).
- **Updates (v1):** manual updates via USB/Arduino IDE (OTA deferred).

## Goals

- First-time user can set up ParkPal with no tooling beyond a phone/laptop browser:
  - connect device to Wi‑Fi
  - enter their own Worker API URL (self-host)
  - deploy their Worker and set `OWM_API_KEY`
  - optional: set trip name/date and pick parks/rides
- Device stays functional even if the backend is down (clear errors, retry backoff).
- Adding more parks/providers later doesn’t require firmware rewrites.
- Firmware shows its version for debugging/support.

## Non-goals (v1)

- Arbitrary city weather or multi-city itineraries.
- Accounts, payments, tiering, email workflows.
- Deep on-device caching of historical wait times.

---

# Architecture (v1 target)

## ESP32 firmware responsibilities

1. **Provisioning mode (first boot / reset)**
   - Runs a Wi‑Fi AP + captive portal UI.
   - Saves provisioning settings to NVS as separate keys (not inside the main JSON config blob):
     - `wifi_ssid`, `wifi_pass`
     - `api_base_url` (required: builder’s self-hosted Worker URL)
2. **Normal mode**
   - Cycles through enabled parks (one park per refresh).
   - Requests summary for exactly one park per update.
3. **Manual update support**
   - Display current firmware version on the screen and in the web UI footer.

## Worker API responsibilities

- Cache per-park payloads to minimize upstream and cost.
- Normalize Queue-Times schema differences into a stable output shape.

## ESP32 local endpoints (kept)

Keep the current pattern where the ESP32 serves:
- UI at `/`
- config at `/api/config`
- refresh trigger at `/api/refresh`

Add/keep proxy endpoints so the browser never needs CORS or to know backend details:
- `/api/destinations` → proxies `{api_base_url}/v1/destinations`
- `/api/rides?park=...` → proxies `{api_base_url}/v1/rides?park=...`
- (optional) `/api/status` → proxies `{api_base_url}/v1/status`

Firmware can either:
- call Worker directly (as it does today for summary), or
- reuse the same proxy helpers internally (preferred for consistency and for supporting `api_base_url` in one place).

---

# Config storage (ESP32 NVS)

ParkPal stores two kinds of settings:

1. **Provisioning keys** (written by the captive portal before any other config exists)
   - `wifi_ssid` (string)
   - `wifi_pass` (string)
   - `api_base_url` (string; **required**, no public default)
2. **Preferences JSON blob** (everything else, stored under one key like `config_json`)
   - `mode`, `parks_enabled`, `rides_by_park_*`, `trip_*`, `countdowns_*`, etc.
   - `resort` is **deprecated** in v1: ignore it at runtime (it may exist only for migrating older configs/UI convenience).

Migration rule:
- If provisioning keys are missing/blank (especially `api_base_url`), show a “Needs setup” screen and keep provisioning UI available.

Factory reset (authoritative):
- Wipe provisioning keys (`wifi_*`, `api_base_url`).
- Also wipe `config_json` (recommended for a clean “new builder” experience).

---

# Destinations & parks registry

Create a canonical registry used by both:
- ESP32 UI (what parks can be selected)
- Worker (what to fetch, where weather coords are)

Data you need per park:
- `park_id` (your canonical ID; can equal Queue-Times park id)
- `provider`: `"queue_times"`
- `queue_times_url`
- `name`, `destination`
- `coords` (lat/lon) for park-specific weather (fixes “Japan” Tokyo vs Osaka)
- `tz` (IANA or POSIX TZ string used by firmware for date math)

Keep destinations simple and curated:
- Orlando: Walt Disney World (WDW) parks
- California: Disneyland Resort (Disneyland Park + Disney California Adventure)
- Tokyo: Tokyo Disney Resort (Tokyo Disneyland + Tokyo DisneySea)

Future (explicitly deferred in v1):
- Universal parks
- Disneyland Paris

---

# Worker API changes

## API base URL + versioning

- Device stores `api_base_url` without a trailing slash (normalize on save).
- All endpoints are rooted at: `{api_base_url}/v1/...`
- Reserve `/v2/...` for breaking changes later.

## Cloudflare bindings (v1, authoritative)

Worker requires:
- Secret/env var: `OWM_API_KEY`
- Optional env vars:
  - `UPSTREAM_TIMEOUT_MS` (defaults to 4000 if unset)

If using `wrangler.toml`, the minimal shape is:
```toml
name = "parkpal-api"
main = "worker.js"
compatibility_date = "2026-02-10"

[vars]
UPSTREAM_TIMEOUT_MS = "4000"
```

Notes:
- No auth is required in v1 because each user hosts their own Worker.
- If you later decide to host a shared/public API, add tokens + rate limiting at that time (do not prebuild it now).

## `/v1/regions` endpoint fate (v1)

The current Worker exposes `GET /v1/regions`. For v1:
- Implement `GET /v1/destinations` as the canonical endpoint (used by firmware/UI).
- **Decision (v1):** keep `GET /v1/regions` as a deprecated alias that returns the **same payload** as `/v1/destinations`.
  - Rationale: avoids breaking older forks and costs almost nothing to maintain.

## Plan vs current code (delta summary)

This repo currently works for a single personal device. The public v1 requires these big deltas:

Firmware:
- Remove hardcoded Wi‑Fi creds (`WIFI_SSID`/`WIFI_PASS`) and any personal defaults in `DEFAULT_CONFIG`.
- Replace hardcoded backend URLs (`API_SUMMARY`/`API_RIDES`) with configurable `api_base_url` stored in NVS.
- Add AP + captive portal provisioning (state machine + DNS redirect + minimal setup page).
- Add BOOT long-press factory reset while running.

Worker:
- Today’s Worker is region-based. The v1 plan moves to per-park caching and a park-scoped summary.
- This is a **breaking change**: update Worker first, then update firmware to match.
- For v1 launch, remove or disable region-based replay + scheduled warm-cache logic to keep the Worker small and easy to understand.

## Caching strategy

Replace “cache by region” with **cache by park**:
- key: `park_id + units` (and provider version)
- store payload:
  - rides list
  - weather for that park coords
  - `updated_at`

This avoids fetching every park when only one is shown.

### Cache keys (authoritative)

Cache API synthetic URL keys:
- Summary: `https://cache.parkpal.fun/v1/summary?park=<id>&units=<metric|imperial>`
- Rides list: `https://cache.parkpal.fun/v1/rides?park=<id>`

Note:
- `cache.parkpal.fun` is a synthetic URL used only as a Cache API key. It does not need to exist or be owned.

TTL:
- summary: 30 minutes
- rides list: 24 hours

## Endpoints (v1 contract)

- All endpoints are public (no auth) in v1 because each user hosts their own Worker.
- JSON responses must include `errors: []` even when empty (stable shape).

### `GET /v1/destinations`

Purpose: UI destination/park picker.

Response 200:
```json
{
  "updated_at": "2026-02-10T12:00:00.000Z",
  "destinations": [
    {
      "id": "orlando",
      "name": "Orlando",
      "parks": [
        { "id": 6, "name": "Magic Kingdom", "provider": "queue_times" }
      ]
    }
  ],
  "errors": []
}
```

### `GET /v1/health`

Purpose: cheap health check for uptime monitoring and device troubleshooting.

Response 200:
```json
{ "ok": true, "time": "2026-02-10T12:00:00.000Z" }
```

### `GET /v1/rides?park=<id>`

Purpose: UI ride picker.

Response 200:
```json
{
  "updated_at": "2026-02-10T12:00:00.000Z",
  "park": { "id": 274, "name": "Tokyo Disneyland" },
  "rides": [
    { "id": 123, "name": "Ride Name" }
  ],
  "errors": []
}
```

### `POST /v1/summary`

Purpose: device display refresh for **one park**.

Request body (JSON):
```json
{
  "park": 274,
  "units": "metric",
  "favorite_ride_ids": [123, 456, 789, 101112, 131415, 161718]
}
```

Rules:
- `park` required (int).
- `units` optional (`metric|imperial`, default `imperial`).
- `favorite_ride_ids` required (max 6). If missing → 400.
- If `favorite_ride_ids` is present but empty, return `rides: []` (device can fall back to labels / show closed).
- Unknown/extra fields in the request body should be ignored (forward compatible).

Response 200:
```json
{
  "updated_at": "2026-02-10T12:00:00.000Z",
  "server_time": "2026-02-10T12:00:01.000Z",
  "units": "metric",
  "park": {
    "id": 274,
    "name": "Tokyo Disneyland",
    "rides": [
      { "id": 123, "name": "Ride", "is_open": true, "wait_time": 25 }
    ]
  },
  "weather": { "temp": 7, "desc": "overcast clouds", "sunrise": 0, "sunset": 0 },
  "errors": [],
  "source": "cache"
}
```

Common errors:
- `400` `{ "error": "bad_request", "details": "missing park" }`
- `503` `{ "error": "upstream_error" }`

---

# Firmware provisioning UX (AP + captive portal)

## States

- **Unprovisioned:** no Wi‑Fi creds or no `api_base_url` → start AP mode automatically.
- **Provisioned:** connect to Wi‑Fi; show IP + `parkpal.local` and normal UI.
- **Error:** Wi‑Fi failed repeatedly → fall back to AP mode after **5 minutes** of disconnected time.

## Required fields on setup page

- Wi‑Fi SSID + password
- Worker API base URL (`api_base_url`)

## Reset

- Use the ESP32 board buttons:
  - `EN` is reset (cannot be used as an input; it reboots the MCU).
  - `BOOT` is typically `GPIO0` and can be read as a digital input **after boot**.
    - Note: holding `GPIO0` low during reset usually enters the ROM serial bootloader (firmware will not run).
- Reset / provisioning behavior (authoritative):
  - **Factory reset:** hold `BOOT` for 8 seconds while the firmware is running → wipe Wi‑Fi + `api_base_url` + optional user settings → reboot into provisioning.
  - **Enter provisioning without wiping:** provide a UI button like “Enter Setup Mode” (recommended), or define a shorter `BOOT` press gesture handled during normal runtime.
- Provisioning AP must be **WPA2-protected** with a random password shown on the e‑ink screen to prevent sniffing during setup.
  - Password generation (authoritative): use `esp_random()` (not `rand()`/`millis()` seeding) to generate a 12+ character alphanumeric password.

---

# Phases & acceptance criteria

## Phase 0 — Repo + docs

- [ ] Remove hardcoded secrets from firmware defaults.
- [ ] Add MIT `LICENSE` + README quickstart + this doc.
- [ ] Document third-party dependencies and caveats.

Concrete “do not ship until done” checklist (Phase 0):
- [ ] Remove `WIFI_SSID` / `WIFI_PASS` (currently `parkpal.ino:20-21`).
- [ ] Remove hardcoded personal backend URLs (`API_SUMMARY`, `API_RIDES`, currently `parkpal.ino:23-24`) or make them placeholders.
- [ ] Remove/replace personal defaults in `DEFAULT_CONFIG` (currently `parkpal.ino:115`, includes names/birthdays).
- [ ] Add `.gitignore` for local artifacts (examples: `.dev.vars`, `node_modules/`, build outputs).
- [ ] Fill in real BOM links + real photos (README). No placeholders.
- [ ] Add MIT `LICENSE` (recommended for maximum community adoption).

## Phase 1 — Provisioning + self-host URL

- [ ] AP mode starts when unprovisioned (no Wi‑Fi or no `api_base_url`).
- [ ] Captive portal DNS redirects to setup page.
- [ ] Setup page collects Wi‑Fi SSID/password + Worker URL only (keep it minimal).
- [ ] Implement the captive portal HTML as a separate minimal page (new file suggested: `setup_html.h`) so it stays small and reliable.
- [ ] Settings save to NVS and reboot into STA mode.
- [ ] Wi‑Fi creds + `api_base_url` persist to NVS.
- [ ] `BOOT`/`EN` behavior works as specified (enter provisioning; factory reset).
- [ ] Device shows a clear error if the backend is unreachable/misconfigured.

Design note (important):
- The captive portal should **not** depend on Worker reachability. Only collect Wi‑Fi + Worker URL.
- After provisioning, the normal web UI can fetch `/api/destinations` to populate park choices.

## Phase 2 — Worker per-park caching + park-weather

- [ ] Worker fetches only the requested park per refresh.
- [ ] Weather is correct for Tokyo vs Osaka in “Japan”.
- [ ] UI ride picker works for all destinations.

## Phase 3 — Shareability polish

- [ ] Nice enclosure/print files + build guide.
- [ ] Demo instructions and screenshots for socials/Reddit.

---

# Implementation checklist (step-by-step, “just execute”)

This section is intentionally prescriptive.

## Repo source-of-truth files (create/adjust)

- `parks.json` (new): canonical list of destinations/parks, including:
  - park id, display name, provider/url, coords, destination id/name
  - This file is the source for Worker `/v1/destinations`.

Important:
- Queue-Times park IDs must be verified (don’t guess). Add the California Disneyland Resort park IDs before calling v1 “launch ready”.

Minimal `parks.json` shape:
```json
{
  "destinations": [
    {
      "id": "california",
      "name": "California",
      "parks": [
        {
          "id": 123,
          "name": "Disneyland Park",
          "provider": "queue_times",
          "queue_times_url": "https://queue-times.com/parks/123/queue_times.json",
          "coords": { "lat": 0, "lon": 0 },
          "tz": "PST8PDT,M3.2.0/2,M11.1.0/2"
        }
      ]
    }
  ]
}
```

Consumption (authoritative):
- Worker imports it at build time (Wrangler bundles JSON imports). Example:
  - `import parks from "./parks.json";`
- Firmware does **not** embed this list; it fetches `/v1/destinations` after provisioning.

## Worker (Cloudflare)

0. Remove deferred Worker features (keep v1 small):
   - Delete the `scheduled()` handler and any cron trigger config (don’t prefetch/warm-cache in v1).
   - Delete/disable region-based replay/history helpers (example names from current code: `lastOpenCacheKey`, `historyCacheKey`, `historyBucketCacheKey`, and any “replay” branches in summary fetch).
   - Goal: Worker only fetches on demand for the requested park.

1. Deploy the Worker to your own Cloudflare account.
2. Set secret `OWM_API_KEY`.
3. Implement `/v1/destinations` by reading `parks.json` and grouping by destination.
4. Refactor per-park fetch:
   - fetch Queue-Times JSON for the park URL
   - normalize rides from `lands[].rides` + `rides[]`
   - fetch OWM weather using park-specific coords
   - store cache entries using the keys above
5. Implement endpoints exactly as specified (status codes + JSON shapes).

## Firmware (ESP32)

1. Add a provisioning NVS key: `api_base_url` (separate from the preferences JSON blob).
2. Add provisioning state machine:
   - if missing Wi‑Fi or `api_base_url` → AP + captive portal
   - if Wi‑Fi fails repeatedly → AP fallback
3. Add UI fields (normal mode UI + captive portal):
   - API base URL override
   - Destination/park selection driven by Worker `/v1/destinations` (v1: Disney only — Orlando/California/Tokyo).
4. Captive portal mechanics (authoritative):
   - Start `WiFi.softAP()` (WPA2) with SSID shown on the display.
   - Run a DNS server that resolves **all** hostnames to the captive portal IP.
   - Serve a minimal setup page (from `setup_html.h`) that only collects Wi‑Fi + Worker URL.
5. Update HTTP client:
   - base URL configurable
6. Update summary call contract:
   - POST one park + favorite ride IDs (always 6)
7. Add a “Firmware version” readout in the web UI and/or a small footer line on the display.

## Manual smoke tests

Worker:
- `/v1/rides` returns rides for at least one park per destination
- `/v1/summary` returns only favorites and includes weather

Device:
- fresh device → AP portal appears
- after provisioning → connects and shows parks screen

---

# Appendix: Draft README (v1)

This is a draft README you can paste into `README.md` and tweak. Keep the tone light and honest.

## Photos / Screenshots (placeholders)

Add these near the top of the real `README.md`:

- Hero photo (device on desk): `docs/images/hero.jpg`
- Close-up photo (e‑ink screen readable): `docs/images/screen-closeup.jpg`
- Web UI screenshot (mobile): `docs/images/ui-mobile.png`
- Web UI screenshot (desktop): `docs/images/ui-desktop.png`
- Optional demo GIF/video:
  - `docs/images/demo.gif` (small) or
  - link to a short video in the README

## Title

`# ParkPal — Disney E‑Ink Wait Times + Trip Countdown (ESP32)`

## Hero paragraph

ParkPal is an ESP32 + 7.5" tri-color e‑ink dashboard that shows:
- Disney park wait times
- a trip countdown (“131 DAYS UNTIL JAPAN”)

It refreshes on a timer, looks great on a desk, and makes you want to book flights.

## What It Does

- Cycles through the parks you pick (Orlando, California, Tokyo).
- For each park, shows your 6 favorite rides + wait times (or “Closed”).
- Shows weather for the currently displayed park.
- Shows a trip countdown if enabled.
- Hosts a tiny web UI at `http://parkpal.local/` for configuration.

That’s it. That’s the project.

## What You Need (BOM)

**Recommended build (what I used):**
- ESP32 e‑paper driver board (with USB‑serial)
- 7.5" tri‑color e‑ink panel (880×528)
- USB cable + (optional) frame/case

**Links:**
- Board: *(add your AliExpress link)*
- Panel: *(add your AliExpress link)*

**Nice-to-haves:**
- A frame or 3D printed enclosure
- A LiPo battery (if your board supports it)

## How Hard Is This?

If you can follow instructions, you can build this.

No coding required unless you want to customize it. The “hardest” part is setting up a free Cloudflare Worker + an OpenWeather API key (5–10 minutes).

## Setup (Short Version)

1. Flash the firmware to the ESP32
2. Deploy the Cloudflare Worker
3. Create an OpenWeather key and set `OWM_API_KEY` in your Worker
4. Power on ParkPal → connect to the setup Wi‑Fi → enter your Wi‑Fi + Worker URL
5. Open `http://parkpal.local/` → pick parks + rides

Done. Enjoy the countdown.

## Firmware Build (Arduino IDE)

You’ll need:
- Arduino IDE
- ESP32 board support by Espressif (“esp32” in Boards Manager)

Libraries (Arduino Library Manager):
- `GxEPD2` (e‑ink driver)
- `ArduinoJson` (v6.x; config + payload parsing)
- `ESPAsyncWebServer` (commonly “ESP Async WebServer” by me-no-dev / community forks)
- `AsyncTCP` (dependency of ESPAsyncWebServer on ESP32)

Board settings:
- Pick the ESP32 board that matches your driver board (many use “ESP32 Dev Module”).
- If uploads fail, double-check the port/cable and try a lower upload speed.
- If you hit “Sketch too big”, pick a partition scheme with a larger app slot.

## Hardware: Plug-In Wiring

Good news: the display plugs into the driver board with a ribbon cable (no soldering).

- Photo of the cable/connector: *(add photo)*
- Photo of the back of the frame/case: *(add photo)*

Firmware pin notes (only matters if you swap boards):
- Defaults in `parkpal.ino`:
  - `EPD_CS = 15`
  - `EPD_DC = 27`
  - `EPD_RST = 26`
  - `EPD_BUSY = 25`
  - `EPD_SCK = 13`
  - `EPD_MOSI = 14`

## Deploy the Backend (Cloudflare Worker)

ParkPal does **not** use a shared public API. Each builder deploys their own Worker.

Short version:

1. Install Wrangler: `npm install -g wrangler`
2. Login: `wrangler login`
3. Deploy from the repo root: `wrangler deploy`
4. Set your OpenWeather key: `wrangler secret put OWM_API_KEY`
   - Get a key at https://openweathermap.org/api
5. Copy the printed Worker URL — you’ll paste this into ParkPal setup.

Local development note:
- Use `.dev.vars` for local secrets (it should be in `.gitignore`).
- Do not put `OWM_API_KEY` in `wrangler.toml`.
- You shouldn’t need a `package.json` for this Worker; `wrangler deploy` from the repo root should work with `wrangler.toml` + `worker.js` + `parks.json`.

## First Boot / Setup Mode

On first boot, ParkPal starts a setup Wi‑Fi network and a captive portal:
- Join the ParkPal setup network (name shown on the display)
- Enter your home Wi‑Fi + your Worker URL
- ParkPal reboots and connects

Reset to setup:
- Hold `BOOT` for ~8 seconds while it’s running to factory reset.

## Buttons

| Button | What it does |
|--------|--------------|
| EN | Reset (reboots) |
| BOOT | Long-press = factory reset to setup mode |

## Troubleshooting

- Can’t connect to `parkpal.local` → use the IP shown on the display.
- Android note: many Android devices don’t resolve `.local` mDNS names; use the IP address.
- Weather is blank → check `OWM_API_KEY` is set in your Worker.
- All rides show Closed → parks might be closed (or upstream is down); try again later.

## What’s In This Repo

```
parkpal/
├── parkpal.ino      ← ESP32 firmware
├── html.h           ← Web UI served by the ESP32
├── setup_html.h     ← Minimal captive-portal setup page (planned)
├── worker.js        ← Cloudflare Worker (self-hosted)
├── parks.json       ← Destination/park registry (planned)
├── wrangler.toml    ← Cloudflare Worker config (planned)
├── WeatherIcons.h   ← Bitmap icons
├── IMPLEMENTATION.md
└── README.md
```

## License / Disclaimer

- License: MIT.
- Disney is a trademark of The Walt Disney Company. (Universal support is roadmap; their marks belong to their respective owners.)
- Uses third-party data sources (Queue-Times, OpenWeather); their uptime/terms apply.
