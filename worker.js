// ParkPal API – live data + edge cache (no DB needed)
// Supports multiple regions: Orlando (WDW) + Tokyo Disney Resort

import parksRegistry from "./parks.json";

// --- Tunables (can override via env vars if you want) ---
const DEFAULT_TIMEOUT_MS = 4000; // 4s
const CACHE_TTL_SECONDS = 1800;  // 30 minutes
const RIDES_CACHE_TTL_SECONDS = 86400; // 24 hours
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

// Flat list of all parks (for lookups — used by summary, which still reads REGIONS)
const ALL_PARKS = Object.values(REGIONS).flatMap(r => r.parks);

// Flat park lookup from parks.json registry (used by rides endpoint)
const REGISTRY_PARKS = new Map();
for (const dest of parksRegistry.destinations) {
  for (const p of dest.parks) REGISTRY_PARKS.set(p.id, p);
}

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

    // --- Cache status per park (no upstream calls) ---
    if (url.pathname === "/v1/status") {
      const now = Date.now();
      const describe = (p) => p ? {
        present: true,
        updated_at: p.updated_at || null,
        age_seconds: p.updated_at ? Math.max(0, Math.floor((now - new Date(p.updated_at).getTime()) / 1000)) : null,
        errors: Array.isArray(p.errors) ? p.errors : []
      } : { present: false };

      const parks = {};
      for (const [parkId] of REGISTRY_PARKS) {
        const imp = await cacheGetParkSummary(parkId, "imperial");
        const met = await cacheGetParkSummary(parkId, "metric");
        parks[parkId] = { imperial: describe(imp), metric: describe(met) };
      }
      return json({ now: new Date(now).toISOString(), parks }, 0, { "x-request-id": requestId, ...CORS });
    }

    // --- Canonical ride list for one park
    // GET /v1/rides?park=274
    if (req.method === "GET" && url.pathname === "/v1/rides") {
      const parkParam = url.searchParams.get("park");
      const parkId = Number(parkParam);
      if (!parkParam || !Number.isInteger(parkId)) {
        return json({ error: "bad_request", details: "missing park" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      const parkEntry = REGISTRY_PARKS.get(parkId);
      if (!parkEntry) {
        return json({ error: "bad_request", details: "unknown park" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      // Try rides cache first (24h TTL)
      let cached = await cacheGetRides(parkId);
      if (cached) {
        return json(cached, 60, { "x-request-id": requestId, ...CORS });
      }

      // Fetch live from Queue-Times
      const payload = await fetchAndCacheRides(env, parkId, parkEntry);
      if (!payload) {
        return json({ error: "upstream_error" }, 0, { status: 503, "x-request-id": requestId, ...CORS });
      }
      return json(payload, 60, { "x-request-id": requestId, ...CORS });
    }

    // --- Main endpoint: summary for one park
    // POST /v1/summary
    // Body: { park: 274, units?: "metric"|"imperial", favorite_ride_ids: [123, 456, ...] }
    if (req.method === "POST" && url.pathname === "/v1/summary") {
      let body = {};
      try { body = await req.json(); }
      catch (_) {
        return json({ error: "bad_request", details: "invalid JSON" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      const parkId = Number(body.park);
      if (!body.park || !Number.isInteger(parkId)) {
        return json({ error: "bad_request", details: "missing park" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      const parkEntry = REGISTRY_PARKS.get(parkId);
      if (!parkEntry) {
        return json({ error: "bad_request", details: "unknown park" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      if (!Array.isArray(body.favorite_ride_ids)) {
        return json({ error: "bad_request", details: "missing favorite_ride_ids" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }
      if (body.favorite_ride_ids.length > 6) {
        return json({ error: "bad_request", details: "too many favorite_ride_ids" }, 0, { status: 400, "x-request-id": requestId, ...CORS });
      }

      const units = normUnits(body.units);
      const favs = new Set(body.favorite_ride_ids.map(Number).filter(Number.isInteger));

      // Try per-park summary cache (30 min TTL)
      let cacheWasHit = false;
      let payload = await cacheGetParkSummary(parkId, units);

      if (payload) {
        cacheWasHit = true;
      } else {
        payload = await fetchParkSummary(env, parkId, parkEntry, units);
      }

      if (!payload) {
        return json({ error: "upstream_error" }, 0, { status: 503, "x-request-id": requestId, ...CORS });
      }

      // Filter rides to favorites (empty favorites → empty rides)
      const rides = favs.size
        ? (payload.rides || []).filter(r => favs.has(Number(r.id)))
        : [];

      return json({
        updated_at: payload.updated_at,
        server_time: new Date().toISOString(),
        units,
        park: { id: parkId, name: parkEntry.name, rides },
        weather: payload.weather,
        errors: payload.errors || [],
        source: cacheWasHit ? "cache" : "live"
      }, 60, {
        "x-parkpal-cache": cacheWasHit ? "HIT" : "MISS",
        "x-request-id": requestId,
        ...CORS
      });
    }

    // --- Destinations (canonical) + regions (deprecated alias, same payload)
    if (req.method === "GET" && (url.pathname === "/v1/destinations" || url.pathname === "/v1/regions")) {
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

function ridesCacheKey(parkId) {
  return `https://cache.parkpal.fun/${CACHE_VERSION}/rides?park=${parkId}`;
}

async function cacheGetRides(parkId) {
  const key = ridesCacheKey(parkId);
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
    const expiresAtMs = updatedAtMs + RIDES_CACHE_TTL_SECONDS * 1000;
    if (expiresAtMs <= now) return null;
    MEM_CACHE.set(key, { expiresAtMs, payload });
    return payload;
  } catch (_) {
    return null;
  }
}

async function cachePutRides(parkId, payload) {
  const key = ridesCacheKey(parkId);
  MEM_CACHE.set(key, { expiresAtMs: Date.now() + RIDES_CACHE_TTL_SECONDS * 1000, payload });
  const resp = new Response(JSON.stringify(payload), {
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": `public, max-age=${RIDES_CACHE_TTL_SECONDS}`
    }
  });
  await caches.default.put(new Request(key), resp);
}

// Fetch rides for a single park from Queue-Times, normalize, and cache
async function fetchAndCacheRides(env, parkId, parkEntry) {
  try {
    const j = await fetchJSON(parkEntry.queue_times_url, { headers: { "User-Agent": "ParkPal/1.0" } }, env.PREFETCH_TIMEOUT_MS);
    const byId = new Map();

    // Normalize: merge lands[].rides + top-level rides[]
    if (j && Array.isArray(j.lands)) {
      for (const land of j.lands) {
        for (const ride of (land?.rides || [])) {
          if (!ride || ride.id == null) continue;
          byId.set(Number(ride.id), { id: Number(ride.id), name: ride.name || "Unknown Ride" });
        }
      }
    }
    if (j && Array.isArray(j.rides)) {
      for (const ride of j.rides) {
        if (!ride || ride.id == null) continue;
        const id = Number(ride.id);
        if (!byId.has(id)) byId.set(id, { id, name: ride.name || "Unknown Ride" });
      }
    }

    const payload = {
      updated_at: new Date().toISOString(),
      park: { id: parkId, name: parkEntry.name },
      rides: [...byId.values()],
      errors: []
    };
    try { await cachePutRides(parkId, payload); } catch (_) { }
    return payload;
  } catch (_) {
    return null;
  }
}

function parkSummaryCacheKey(parkId, units) {
  return `https://cache.parkpal.fun/${CACHE_VERSION}/summary?park=${parkId}&units=${encodeURIComponent(units)}`;
}

async function cacheGetParkSummary(parkId, units) {
  const key = parkSummaryCacheKey(parkId, units);
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

async function cachePutParkSummary(parkId, units, payload) {
  const key = parkSummaryCacheKey(parkId, units);
  MEM_CACHE.set(key, { expiresAtMs: Date.now() + CACHE_TTL_SECONDS * 1000, payload });
  const resp = new Response(JSON.stringify(payload), {
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": `public, max-age=${CACHE_TTL_SECONDS}`
    }
  });
  await caches.default.put(new Request(key), resp);
}

// Fetch rides + weather for one park from upstream, normalize, and cache
async function fetchParkSummary(env, parkId, parkEntry, units) {
  const errors = [];

  // Fetch rides from Queue-Times
  let rides = [];
  try {
    const j = await fetchJSON(parkEntry.queue_times_url, { headers: { "User-Agent": "ParkPal/1.0" } }, env.PREFETCH_TIMEOUT_MS);
    const byId = new Map();

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
        if (!byId.has(id)) {
          byId.set(id, {
            id,
            name: ride.name || "Unknown Ride",
            is_open: !!ride.is_open,
            wait_time: Number(ride.wait_time ?? 0)
          });
        }
      }
    }
    rides = [...byId.values()];
  } catch (e) {
    errors.push(`park_${parkId}_${e?.status ? `HTTP_${e.status}` : (e?.message || "error")}`);
  }

  // Fetch weather using park-specific coords
  let weather = { temp: 0, desc: "", sunrise: 0, sunset: 0 };
  try {
    const { lat, lon } = parkEntry.coords;
    const w = await fetchJSON(
      `https://api.openweathermap.org/data/2.5/weather?lat=${lat}&lon=${lon}&units=${units}&appid=${env.OWM_API_KEY}`,
      {},
      env.PREFETCH_TIMEOUT_MS
    );
    weather = {
      temp: Math.round(w?.main?.temp ?? 0),
      desc: (w?.weather?.[0]?.description || "").toLowerCase(),
      sunrise: w?.sys?.sunrise ?? 0,
      sunset: w?.sys?.sunset ?? 0
    };
  } catch (e) {
    errors.push(`weather_${e?.status ? `HTTP_${e.status}` : (e?.message || "error")}`);
  }

  const payload = { updated_at: new Date().toISOString(), rides, weather, errors };
  try { await cachePutParkSummary(parkId, units, payload); } catch (_) { }
  return payload;
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
