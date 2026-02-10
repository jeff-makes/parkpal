// ParkPal API – live data + edge cache (no DB needed)
// Supports multiple regions: Orlando (WDW) + Tokyo Disney Resort

import parksRegistry from "./parks.json";

// --- Tunables (can override via env vars if you want) ---
const DEFAULT_TIMEOUT_MS = 4000; // 4s
const CACHE_TTL_SECONDS = 1800;  // 30 minutes
const CACHE_VERSION = "v1";

// In-isolate hot cache (avoids even Cache API lookups when the Worker stays warm)
const MEM_CACHE = new Map(); // key -> { expiresAtMs, payload }

// --- Regions config ---
const REGIONS = {
  orlando: {
    coords: { lat: 28.3772, lon: -81.5707 },
    parks: [
      { id: 6, name: "Magic Kingdom", url: "https://queue-times.com/parks/6/queue_times.json" },
      { id: 7, name: "Hollywood Studios", url: "https://queue-times.com/parks/7/queue_times.json" },
      { id: 8, name: "Animal Kingdom", url: "https://queue-times.com/parks/8/queue_times.json" },
      { id: 5, name: "EPCOT", url: "https://queue-times.com/parks/5/queue_times.json" }
    ]
  },
  tokyo: {
    coords: { lat: 35.6329, lon: 139.8804 },
    parks: [
      { id: 274, name: "Tokyo Disneyland", url: "https://queue-times.com/parks/274/queue_times.json" },
      { id: 275, name: "Tokyo DisneySea", url: "https://queue-times.com/parks/275/queue_times.json" }
    ]
  }
};

// Flat list of all parks (for lookups)
const ALL_PARKS = Object.values(REGIONS).flatMap(r => r.parks);

export default {
  async fetch(req, env, ctx) {
    const url = new URL(req.url);
    const requestId = req.headers.get("cf-ray") || crypto.randomUUID();

    // --- CORS (preflight) ---
    const CORS = {
      "access-control-allow-origin": "*",
      "access-control-allow-methods": "GET, POST, OPTIONS",
      "access-control-allow-headers": "content-type, x-request-id"
    };
    if (req.method === "OPTIONS") {
      return new Response(null, { headers: { ...CORS, "Access-Control-Max-Age": "86400" } });
    }

    // --- Health (no upstream calls; fast) ---
    if (url.pathname === "/v1/health") {
      return json({ ok: true, time: new Date().toISOString() }, 0, { "x-request-id": requestId, ...CORS });
    }

    // --- Cache status (no upstream calls) ---
    if (url.pathname === "/v1/status") {
      const now = Date.now();
      const describe = (p) => p ? {
        present: true,
        updated_at: p.updated_at || null,
        age_seconds: p.updated_at ? Math.max(0, Math.floor((now - new Date(p.updated_at).getTime()) / 1000)) : null,
        errors: Array.isArray(p.errors) ? p.errors : []
      } : { present: false };

      const status = { now: new Date(now).toISOString(), regions: {} };
      for (const region of Object.keys(REGIONS)) {
        const imp = await cacheGetSummaryPayload("imperial", region);
        const met = await cacheGetSummaryPayload("metric", region);
        status.regions[region] = { imperial: describe(imp), metric: describe(met) };
      }
      return json(status, 0, { "x-request-id": requestId, ...CORS });
    }

    // --- Canonical ride list (IDs + names)
    // GET /v1/rides?park=6            (single)
    // GET /v1/rides?park=6,5,7,8      (multi)
    // GET /v1/rides?region=tokyo      (all parks in region)
    // Optional: &include_single_rider=1
    if (req.method === "GET" && url.pathname === "/v1/rides") {
      const regionParam = url.searchParams.get("region");
      const parkParam = url.searchParams.get("park");
      const includeSingle = url.searchParams.get("include_single_rider") === "1";

      let requestedIds = [];

      if (regionParam && REGIONS[regionParam]) {
        // Get all parks for region
        requestedIds = REGIONS[regionParam].parks.map(p => p.id);
      } else if (parkParam) {
        // Parse specific park IDs
        requestedIds = parkParam
          .split(",")
          .map(s => Number(s.trim()))
          .filter(n => Number.isInteger(n) && ALL_PARKS.some(p => p.id === n));
      } else {
        return json({ error: "missing park or region param" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      if (requestedIds.length === 0) {
        return json({ error: "no valid park IDs" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      // Determine region for cache lookup
      const region = regionParam || regionForParkId(requestedIds[0]);

      // Use cached summary (metric key arbitrarily) or fetch if empty/stale
      let payload = await getOrFetchSummary(env, "metric", region, requestId);

      // If still nothing, fail gracefully
      if (!payload || !Array.isArray(payload.parks)) {
        return json({ error: "no data" }, 0, { status: 503, "x-request-id": requestId, ...CORS });
      }

      // If we have a cached payload but ride lists are empty/missing, refresh once.
      // (This protects against upstream schema differences or older cached payloads.)
      const needsRefresh = requestedIds.some(pid => {
        const park = payload.parks.find(p => Number(p.id) === pid);
        return !park || !Array.isArray(park.rides) || park.rides.length === 0;
      });
      if (needsRefresh) {
        payload = await getOrFetchSummary(env, "metric", region, requestId, { forceRefresh: true });
      }

      const parksOut = [];
      for (const pid of requestedIds) {
        const park = payload.parks.find(p => Number(p.id) === pid) || { id: pid, name: nameForPark(pid), rides: [] };
        const rides = (park.rides || [])
          .filter(r => includeSingle ? true : !String(r.name || "").includes("Single Rider"))
          .map(r => ({ id: r.id, name: r.name || "Unknown Ride" }));
        parksOut.push({ park_id: pid, name: park.name || nameForPark(pid), rides });
      }

      return json({
        updated_at: payload.updated_at || new Date().toISOString(),
        region,
        parks: parksOut
      }, 60, { "x-request-id": requestId, ...CORS });
    }

    // --- Main endpoint: summary
    // POST /v1/summary
    // Body: { region?: "orlando"|"tokyo", units?: "imperial"|"metric", parks?: number[], favorite_ride_ids?: number[] }
    if (req.method === "POST" && url.pathname === "/v1/summary") {
      let body = {};
      try { body = await req.json(); }
      catch (_) {
        return json({ error: "Invalid JSON body" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      const region = normRegion(body.region);
      const units = normUnits(body.units);

      const regionConfig = REGIONS[region];
      const defaultParkIds = regionConfig.parks.map(p => p.id);
      const parkIds = parseIds(body.parks, defaultParkIds, 8);
      const favs = new Set(parseIds(body.favorite_ride_ids, [], 50));

      // try cache first; if empty, fetch and populate
      let cacheWasHit = false;
      let payload = await cacheGetSummaryPayload(units, region);

      if (!payload) {
        payload = await getOrFetchSummary(env, units, region, requestId, { forceRefresh: true });
      } else {
        cacheWasHit = true;
      }

      // If somehow nothing, fail clearly
      if (!payload) {
        return json(
          { region, units, updated_at: new Date().toISOString(), weather: null, parks: [], errors: ["no_payload"] },
          0,
          { status: 503, "x-request-id": requestId, ...CORS }
        );
      }

      // If payload represents a total failure
      if ((!payload.parks || payload.parks.length === 0) &&
        (!payload.weather || payload.weather.temp === 0) &&
        (payload.errors && payload.errors.length > 0)) {
        return json(
          { region, units, updated_at: new Date().toISOString(), weather: null, parks: [], errors: payload.errors },
          0,
          { status: 503, "x-request-id": requestId, ...CORS }
        );
      }

      // filter parks/rides
      // If cached payload looks incomplete for requested parks, refresh once.
      if (cacheWasHit && payload && Array.isArray(payload.parks)) {
        const needsRefresh = parkIds.some(pid => {
          const park = payload.parks.find(p => Number(p.id) === pid);
          return !park || !Array.isArray(park.rides) || park.rides.length === 0;
        });
        if (needsRefresh) {
          cacheWasHit = false;
          payload = await getOrFetchSummary(env, units, region, requestId, { forceRefresh: true });
        }
      }

      let parks = payload.parks.filter(p => parkIds.includes(Number(p.id)));
      if (favs.size) {
        parks = parks
          .map(p => ({
            id: p.id, name: p.name,
            rides: (p.rides || []).filter(r => favs.has(Number(r.id)))
          }))
          .filter(p => p.rides.length);
      }

      return json({
        region,
        units,
        updated_at: payload.updated_at,
        server_time: new Date().toISOString(),
        weather: payload.weather,
        parks,
        errors: payload.errors || [],
        source: cacheWasHit ? "cache" : "live"
      }, 60, {
        "x-parkpal-cache": cacheWasHit ? "HIT" : "MISS",
        "x-request-id": requestId,
        ...CORS
      });
    }

    // --- Destinations (canonical endpoint for UI park picker, sourced from parks.json)
    if (req.method === "GET" && url.pathname === "/v1/destinations") {
      const destinations = parksRegistry.destinations.map(d => ({
        id: d.id,
        name: d.name,
        parks: d.parks.map(p => ({ id: p.id, name: p.name, provider: p.provider }))
      }));
      return json({
        updated_at: new Date().toISOString(),
        destinations,
        errors: []
      }, 300, { "x-request-id": requestId, ...CORS });
    }

    // --- List available regions/parks (legacy)
    if (req.method === "GET" && url.pathname === "/v1/regions") {
      const out = {};
      for (const [name, cfg] of Object.entries(REGIONS)) {
        out[name] = {
          coords: cfg.coords,
          parks: cfg.parks.map(p => ({ id: p.id, name: p.name }))
        };
      }
      return json(out, 300, { "x-request-id": requestId, ...CORS });
    }

    if (url.pathname === "/") return new Response("ParkPal API ok", { headers: { "x-request-id": requestId, ...CORS } });
    return new Response("Not found", { status: 404, headers: { "x-request-id": requestId, ...CORS } });
  }
};

// ---------- helpers ----------

function nameForPark(id) {
  return ALL_PARKS.find(p => p.id === id)?.name || `Park ${id}`;
}

function regionForParkId(id) {
  for (const [name, cfg] of Object.entries(REGIONS)) {
    if (cfg.parks.some(p => p.id === id)) return name;
  }
  return "orlando"; // fallback
}

// Unified JSON fetch with timeout + ok-check
async function fetchJSON(url, options = {}, timeoutMs) {
  const ms = Number(timeoutMs) || DEFAULT_TIMEOUT_MS;
  const signal = AbortSignal.timeout(ms);
  const resp = await fetch(url, { ...options, signal });
  if (!resp.ok) {
    const err = new Error(`HTTP ${resp.status}`);
    err.status = resp.status;
    throw err;
  }
  return resp.json();
}

function summaryCacheKey(units, region) {
  // Stable synthetic URL key for Cache API.
  return `https://cache.parkpal.fun/${CACHE_VERSION}/summary?region=${encodeURIComponent(region)}&units=${encodeURIComponent(units)}`;
}

function parseUpdatedAtMs(payload) {
  const ms = Date.parse(payload?.updated_at);
  return Number.isFinite(ms) ? ms : null;
}

async function cacheGetSummaryPayload(units, region) {
  const key = summaryCacheKey(units, region);
  const now = Date.now();

  const mem = MEM_CACHE.get(key);
  if (mem) {
    if (mem.expiresAtMs > now) return mem.payload;
    MEM_CACHE.delete(key);
  }

  const resp = await caches.default.match(new Request(key));
  if (!resp) return null;
  try {
    const payload = await resp.json();
    const updatedAtMs = parseUpdatedAtMs(payload);
    if (!updatedAtMs) return null;
    const expiresAtMs = updatedAtMs + CACHE_TTL_SECONDS * 1000;
    if (expiresAtMs <= now) return null;
    MEM_CACHE.set(key, { expiresAtMs, payload });
    return payload;
  } catch (_) {
    return null;
  }
}

async function cachePutSummaryPayload(units, region, payload) {
  const key = summaryCacheKey(units, region);
  const now = Date.now();

  MEM_CACHE.set(key, { expiresAtMs: now + CACHE_TTL_SECONDS * 1000, payload });
  const resp = new Response(JSON.stringify(payload), {
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": `public, max-age=${CACHE_TTL_SECONDS}`
    }
  });
  await caches.default.put(new Request(key), resp);
}

async function getOrFetchSummary(env, units, region, requestId = "N/A", { forceRefresh = false } = {}) {
  // 1. Try standard cache
  if (!forceRefresh) {
    const cached = await cacheGetSummaryPayload(units, region);
    if (cached) return cached;
  }

  // 2. Fetch live
  const payload = await prefetch(env, units, region, requestId);

  // 3. Save to cache
  try { await cachePutSummaryPayload(units, region, payload); } catch (_) { }

  return payload;
}

async function prefetch(env, units, region, requestId = "N/A") {
  const errors = [];
  const regionConfig = REGIONS[region];

  if (!regionConfig) {
    return { updated_at: new Date().toISOString(), source: "live", units, region, weather: null, parks: [], errors: ["invalid_region"] };
  }

  // fetch parks in parallel (each park isolated so one failure doesn't kill all)
  const parks = await Promise.all(regionConfig.parks.map(async p => {
    try {
      const j = await fetchJSON(p.url, { headers: { "User-Agent": "ParkPal/1.0" } }, env.PREFETCH_TIMEOUT_MS);
      const byId = new Map();

      // Some parks return rides nested under `lands`; others (notably Tokyo) return a flat `rides` array.
      // A few responses include an empty `lands: []` even when `rides` is populated, so merge both.
      if (j && Array.isArray(j.lands)) {
        for (const land of j.lands) {
          for (const ride of (land?.rides || [])) {
            if (!ride || ride.id == null) continue;
            byId.set(Number(ride.id), {
              id: Number(ride.id),
              name: ride.name || "Unknown Ride",
              is_open: !!ride.is_open,
              wait_time: Number(ride.wait_time ?? 0)
            });
          }
        }
      }

      if (j && Array.isArray(j.rides)) {
        for (const ride of j.rides) {
          if (!ride || ride.id == null) continue;
          const id = Number(ride.id);
          if (byId.has(id)) continue;
          byId.set(id, {
            id,
            name: ride.name || "Unknown Ride",
            is_open: !!ride.is_open,
            wait_time: Number(ride.wait_time ?? 0)
          });
        }
      }

      return { id: p.id, name: j?.park || p.name, rides: [...byId.values()] };
    } catch (e) {
      errors.push(`park_${p.id}_${e?.status ? `HTTP_${e.status}` : (e?.message || "error")}`);
      return { id: p.id, name: p.name, rides: [] };
    }
  }));

  // weather (isolated too) — uses region-specific coords
  let weather = { temp: 0, desc: "", sunrise: null, sunset: null };
  try {
    const { lat, lon } = regionConfig.coords;
    const w = await fetchJSON(
      `https://api.openweathermap.org/data/2.5/weather?lat=${lat}&lon=${lon}&units=${units}&appid=${env.OWM_API_KEY}`,
      {},
      env.PREFETCH_TIMEOUT_MS
    );
    weather = {
      temp: Math.round(w?.main?.temp ?? 0),
      desc: (w?.weather?.[0]?.description || "").toLowerCase(),
      sunrise: w?.sys?.sunrise ?? null,
      sunset: w?.sys?.sunset ?? null
    };
  } catch (e) {
    errors.push(`weather_${e?.status ? `HTTP_${e.status}` : (e?.message || "error")}`);
  }

  const payload = {
    updated_at: new Date().toISOString(),
    source: "live",
    region,
    units,
    weather,
    parks,
    errors
  };

  return payload;
}

const normUnits = (u) => (String(u || "imperial").toLowerCase().startsWith("m") ? "metric" : "imperial");
const normRegion = (r) => (REGIONS[String(r || "").toLowerCase()] ? String(r).toLowerCase() : "orlando");
const parseIds = (arr, fallback, maxLen = 8) => {
  if (!Array.isArray(arr) || arr.length === 0) return fallback;
  return arr.map(n => Number(n)).filter(n => Number.isInteger(n)).slice(0, maxLen);
};

function json(data, maxAge = 60, extraHeaders = {}) {
  const { status, ...headers } = extraHeaders;
  return new Response(JSON.stringify(data), {
    status: status || 200,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": `public, max-age=${maxAge}`,
      ...headers
    }
  });
}
