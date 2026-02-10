// Minimal captive-portal setup page (AP mode).
// Kept intentionally small + dependency-free.
// Served when the device is unprovisioned (no Wi-Fi or no Worker URL).

#pragma once

static const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ParkPal Setup</title>
  <style>
    :root { color-scheme: light; }
    body { font-family: -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Helvetica, Arial, sans-serif; margin: 0; background:#f6f7fb; color:#111; }
    .wrap { max-width: 520px; margin: 0 auto; padding: 20px; }
    .card { background:#fff; border:1px solid #e7e7ef; border-radius: 12px; padding: 16px; box-shadow: 0 1px 10px rgba(0,0,0,0.04); }
    h1 { font-size: 22px; margin: 0 0 6px; }
    p { margin: 0 0 14px; color:#333; line-height: 1.35; }
    label { display:block; font-size: 13px; margin: 10px 0 6px; color:#222; }
    input { width:100%; box-sizing:border-box; padding: 10px 12px; border:1px solid #d6d6e3; border-radius: 10px; font-size: 14px; }
    .hint { font-size: 12px; color:#555; margin-top:6px; }
    button { margin-top: 14px; width:100%; padding: 12px 14px; border-radius: 10px; border:0; background:#111; color:#fff; font-size: 15px; font-weight: 600; }
    button:disabled { opacity: .6; }
    .status { margin-top: 12px; font-size: 13px; }
    code { background:#f1f1f6; padding: 2px 6px; border-radius: 6px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>ParkPal Setup</h1>
      <p>Enter your Wi‑Fi and your Cloudflare Worker URL. ParkPal will reboot when saved.</p>

      <label for="ssid">Wi‑Fi SSID</label>
      <input id="ssid" autocomplete="off" placeholder="MyWiFi" />

      <label for="pass">Wi‑Fi Password</label>
      <input id="pass" type="password" autocomplete="off" placeholder="••••••••" />

      <label for="api">Worker URL</label>
      <input id="api" autocomplete="off" placeholder="https://your-worker.your-subdomain.workers.dev" />
      <div class="hint">Tip: paste the URL printed by <code>wrangler deploy</code>. Don’t include <code>/v1</code>.</div>

      <button id="saveBtn" onclick="save()">Save &amp; Reboot</button>
      <div class="status" id="status"></div>
    </div>
  </div>

  <script>
    async function load() {
      try {
        const res = await fetch('/api/provision');
        const data = await res.json();
        if (data && data.wifi_ssid) document.getElementById('ssid').value = data.wifi_ssid;
        if (data && data.api_base_url) document.getElementById('api').value = data.api_base_url;
      } catch (_) {}
    }

    function setStatus(msg, ok) {
      const el = document.getElementById('status');
      el.textContent = msg;
      el.style.color = ok ? '#126b2f' : '#8b1a1a';
    }

    async function save() {
      const btn = document.getElementById('saveBtn');
      btn.disabled = true;
      setStatus('Saving…', true);
      const body = {
        wifi_ssid: document.getElementById('ssid').value.trim(),
        wifi_pass: document.getElementById('pass').value,
        api_base_url: document.getElementById('api').value.trim()
      };
      try {
        const res = await fetch('/api/provision', {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify(body)
        });
        const data = await res.json().catch(() => ({}));
        if (!res.ok) throw new Error(data.details || data.error || ('HTTP ' + res.status));
        setStatus('Saved! Rebooting…', true);
      } catch (e) {
        setStatus('Error: ' + (e && e.message ? e.message : 'failed'), false);
        btn.disabled = false;
      }
    }
    load();
  </script>
</body>
</html>
)HTML";

