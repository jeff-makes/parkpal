// html.h - ParkPal Web UI
// Supports Walt Disney World (WDW), Disneyland Resort (DLR), and Tokyo Disney Resort (TDR)

static const char INDEX_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
  <title>ParkPal</title>
  <style>
    :root {
      --bg: #f5f5f7;
      --card: #ffffff;
      --text: #1d1d1f;
      --text-secondary: #86868b;
      --accent: #0071e3;
      --accent-hover: #0077ed;
      --border: #d2d2d7;
      --danger: #ff3b30;
      --success: #34c759;
      --radius: 12px;
      --radius-sm: 8px;
      --shadow: 0 2px 8px rgba(0,0,0,0.08);
      --shadow-lg: 0 8px 32px rgba(0,0,0,0.12);
      --transition: 0.2s ease;
    }

    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #000000;
        --card: #1c1c1e;
        --text: #f5f5f7;
        --text-secondary: #98989d;
        --border: #38383a;
        --shadow: 0 2px 8px rgba(0,0,0,0.3);
        --shadow-lg: 0 8px 32px rgba(0,0,0,0.5);
      }
    }

    * {
      box-sizing: border-box;
      -webkit-tap-highlight-color: transparent;
    }

    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
      background: var(--bg);
      color: var(--text);
      margin: 0;
      padding: 16px;
      padding-bottom: 100px;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }

    .container {
      max-width: 600px;
      margin: 0 auto;
    }

    /* Header */
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 24px;
      padding: 8px 0;
    }

    .header h1 {
      font-size: 28px;
      font-weight: 700;
      margin: 0;
      display: flex;
      align-items: center;
      gap: 10px;
    }

    .header h1::before {
      content: "🏰";
      font-size: 32px;
    }

    .status {
      font-size: 13px;
      padding: 6px 12px;
      border-radius: 20px;
      background: var(--card);
      color: var(--text-secondary);
      border: 1px solid var(--border);
    }

    .status.connected {
      background: #d1f2d9;
      color: #1a7f37;
      border-color: #a7d9b2;
    }

    @media (prefers-color-scheme: dark) {
      .status.connected {
        background: #1a3d25;
        color: #56d364;
        border-color: #2d5a3d;
      }
    }

    /* Cards */
    .card {
      background: var(--card);
      border-radius: var(--radius);
      padding: 20px;
      margin-bottom: 16px;
      box-shadow: var(--shadow);
    }

    .card-title {
      font-size: 11px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      color: var(--text-secondary);
      margin: 0 0 16px 0;
    }

    /* Form Elements */
    label {
      display: block;
      margin-bottom: 16px;
    }

    label:last-child {
      margin-bottom: 0;
    }

    .label-text {
      display: block;
      font-size: 15px;
      font-weight: 500;
      margin-bottom: 8px;
    }

    select, input[type="text"], input[type="date"], input[type="number"] {
      width: 100%;
      padding: 12px 14px;
      font-size: 16px;
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      background: var(--bg);
      color: var(--text);
      transition: border-color var(--transition);
      -webkit-appearance: none;
      appearance: none;
    }

    select {
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%2386868b' d='M6 8L1 3h10z'/%3E%3C/svg%3E");
      background-repeat: no-repeat;
      background-position: right 14px center;
      padding-right: 40px;
    }

    select:focus, input:focus {
      outline: none;
      border-color: var(--accent);
    }

    /* Checkbox / Toggle */
    .toggle-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 4px 0;
    }

    .toggle-row .label-text {
      margin: 0;
    }

    .toggle {
      position: relative;
      width: 51px;
      height: 31px;
      flex-shrink: 0;
    }

    .toggle input {
      opacity: 0;
      width: 0;
      height: 0;
    }

    .toggle-slider {
      position: absolute;
      cursor: pointer;
      inset: 0;
      background: var(--border);
      border-radius: 31px;
      transition: var(--transition);
    }

    .toggle-slider::before {
      content: "";
      position: absolute;
      height: 27px;
      width: 27px;
      left: 2px;
      bottom: 2px;
      background: white;
      border-radius: 50%;
      transition: var(--transition);
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }

    .toggle input:checked + .toggle-slider {
      background: var(--success);
    }

    .toggle input:checked + .toggle-slider::before {
      transform: translateX(20px);
    }

    /* Park Checkboxes */
    .park-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
      gap: 10px;
    }

    .park-check {
      display: flex;
      align-items: center;
      padding: 14px;
      background: var(--bg);
      border: 2px solid var(--border);
      border-radius: var(--radius-sm);
      cursor: pointer;
      transition: var(--transition);
      user-select: none;
    }

    .park-check:hover {
      border-color: var(--accent);
    }

    .park-check.checked {
      border-color: var(--accent);
      background: rgba(0, 113, 227, 0.08);
    }

    .park-check input {
      display: none;
    }

    .park-check .checkmark {
      width: 22px;
      height: 22px;
      border: 2px solid var(--border);
      border-radius: 6px;
      margin-right: 10px;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: var(--transition);
      flex-shrink: 0;
    }

    .park-check.checked .checkmark {
      background: var(--accent);
      border-color: var(--accent);
    }

    .park-check.checked .checkmark::after {
      content: "";
      width: 6px;
      height: 10px;
      border: solid white;
      border-width: 0 2px 2px 0;
      transform: rotate(45deg) translateY(-1px);
    }

    .park-check .park-name {
      font-size: 14px;
      font-weight: 500;
    }

    .park-check .park-abbr {
      font-size: 11px;
      color: var(--text-secondary);
    }

    /* Accordion */
    .accordion {
      background: var(--bg);
      border-radius: var(--radius-sm);
      overflow: hidden;
      margin-top: 16px;
    }

    .accordion-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 14px 16px;
      font-weight: 600;
      font-size: 15px;
      cursor: pointer;
      user-select: none;
      transition: background var(--transition);
    }

    .accordion-header:hover {
      background: var(--border);
    }

    .accordion-icon {
      font-size: 12px;
      color: var(--text-secondary);
      transition: transform var(--transition);
    }

    .accordion.open .accordion-icon {
      transform: rotate(180deg);
    }

    .accordion-body {
      display: none;
      padding: 0 16px 16px;
    }

    .accordion.open .accordion-body {
      display: block;
    }

    /* Ride Slots */
    .ride-slot {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 12px;
      background: var(--card);
      border-radius: var(--radius-sm);
      margin-bottom: 8px;
    }

    .ride-slot:last-child {
      margin-bottom: 0;
    }

    .slot-num {
      width: 28px;
      height: 28px;
      background: var(--accent);
      color: white;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 13px;
      font-weight: 600;
      flex-shrink: 0;
    }

    .slot-name {
      flex: 1;
      font-size: 14px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      color: var(--text);
    }

    .slot-name.empty {
      color: var(--text-secondary);
      font-style: italic;
    }

    .slot-btn {
      padding: 8px 14px;
      font-size: 13px;
      font-weight: 500;
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: 6px;
      color: var(--accent);
      cursor: pointer;
      transition: var(--transition);
    }

    .slot-btn:hover {
      background: var(--accent);
      color: white;
      border-color: var(--accent);
    }

    /* Countdown List */
    .countdown-item {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 14px;
      background: var(--bg);
      border-radius: var(--radius-sm);
      margin-bottom: 10px;
    }

    .countdown-item:last-child {
      margin-bottom: 0;
    }

    .cd-check {
      width: 22px;
      height: 22px;
      border: 2px solid var(--border);
      border-radius: 6px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      flex-shrink: 0;
      transition: var(--transition);
    }

    .cd-check.checked {
      background: var(--accent);
      border-color: var(--accent);
    }

    .cd-check.checked::after {
      content: "";
      width: 6px;
      height: 10px;
      border: solid white;
      border-width: 0 2px 2px 0;
      transform: rotate(45deg) translateY(-1px);
    }

    .cd-label {
      flex: 1;
      font-weight: 500;
      font-size: 15px;
    }

    .cd-actions {
      display: flex;
      gap: 8px;
    }

    .cd-actions button {
      padding: 6px 12px;
      font-size: 13px;
      border-radius: 6px;
      border: none;
      cursor: pointer;
      transition: var(--transition);
    }

    .btn-edit {
      background: var(--card);
      border: 1px solid var(--border);
      color: var(--text);
    }

    .btn-delete {
      background: transparent;
      color: var(--danger);
    }

    .btn-delete:hover {
      background: rgba(255, 59, 48, 0.1);
    }

    /* Add Countdown Button */
    .add-countdown {
      width: 100%;
      padding: 14px;
      font-size: 15px;
      font-weight: 500;
      background: transparent;
      border: 2px dashed var(--border);
      border-radius: var(--radius-sm);
      color: var(--accent);
      cursor: pointer;
      transition: var(--transition);
      margin-top: 12px;
    }

    .add-countdown:hover {
      border-color: var(--accent);
      background: rgba(0, 113, 227, 0.05);
    }

    /* Bottom Action Bar */
    .action-bar {
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      padding: 16px;
      background: var(--card);
      border-top: 1px solid var(--border);
      display: flex;
      gap: 12px;
      z-index: 100;
    }

    .action-bar button {
      flex: 1;
      padding: 16px;
      font-size: 16px;
      font-weight: 600;
      border-radius: var(--radius);
      border: none;
      cursor: pointer;
      transition: var(--transition);
    }

    .btn-secondary {
      background: var(--bg);
      color: var(--text);
      border: 1px solid var(--border);
    }

    .btn-primary {
      background: var(--accent);
      color: white;
    }

    .btn-primary:hover {
      background: var(--accent-hover);
    }

    .btn-primary:disabled {
      background: var(--border);
      cursor: not-allowed;
    }

    /* Modal / Drawer */
    .overlay {
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.5);
      opacity: 0;
      visibility: hidden;
      transition: var(--transition);
      z-index: 200;
    }

    .overlay.open {
      opacity: 1;
      visibility: visible;
    }

    .modal {
      position: fixed;
      left: 50%;
      top: 50%;
      transform: translate(-50%, -50%) scale(0.95);
      width: min(90vw, 480px);
      max-height: 85vh;
      background: var(--card);
      border-radius: var(--radius);
      box-shadow: var(--shadow-lg);
      opacity: 0;
      visibility: hidden;
      transition: var(--transition);
      z-index: 201;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }

    .modal.open {
      opacity: 1;
      visibility: visible;
      transform: translate(-50%, -50%) scale(1);
    }

    @media (max-width: 500px) {
      .modal {
        top: auto;
        bottom: 0;
        left: 0;
        right: 0;
        transform: translateY(100%);
        width: 100%;
        max-height: 90vh;
        border-radius: var(--radius) var(--radius) 0 0;
      }

      .modal.open {
        transform: translateY(0);
      }
    }

    .modal-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 16px 20px;
      border-bottom: 1px solid var(--border);
    }

    .modal-header h2 {
      font-size: 17px;
      font-weight: 600;
      margin: 0;
    }

    .modal-close {
      width: 30px;
      height: 30px;
      border-radius: 50%;
      background: var(--bg);
      border: none;
      font-size: 18px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      color: var(--text-secondary);
    }

    .modal-body {
      padding: 20px;
      overflow-y: auto;
      flex: 1;
    }

    .modal-footer {
      padding: 16px 20px;
      border-top: 1px solid var(--border);
    }

    .modal-footer button {
      width: 100%;
      padding: 14px;
      font-size: 16px;
      font-weight: 600;
      background: var(--accent);
      color: white;
      border: none;
      border-radius: var(--radius-sm);
      cursor: pointer;
    }

    /* Ride Picker */
    .search-input {
      width: 100%;
      padding: 14px 16px;
      font-size: 16px;
      border: none;
      border-bottom: 1px solid var(--border);
      background: var(--card);
      color: var(--text);
    }

    .search-input:focus {
      outline: none;
    }

    .ride-list {
      list-style: none;
      margin: 0;
      padding: 0;
    }

    .ride-list li {
      padding: 14px 20px;
      cursor: pointer;
      transition: background var(--transition);
      border-bottom: 1px solid var(--border);
    }

    .ride-list li:last-child {
      border-bottom: none;
    }

    .ride-list li:hover {
      background: var(--bg);
    }

    /* Preset Buttons */
    .preset-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
      margin-bottom: 16px;
    }

    .preset-btn {
      padding: 12px;
      font-size: 14px;
      font-weight: 500;
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      cursor: pointer;
      transition: var(--transition);
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .preset-btn:hover {
      border-color: var(--accent);
      color: var(--accent);
    }

    /* Helper text */
    .helper {
      font-size: 13px;
      color: var(--text-secondary);
      margin-top: 8px;
    }

    /* Section visibility */
    .hidden {
      display: none !important;
    }

    /* Row layout */
    .row {
      display: flex;
      gap: 12px;
    }

    .row > * {
      flex: 1;
    }
  </style>
</head>
<body>
  <div class="container">
    <!-- Header -->
    <div class="header">
      <h1>ParkPal</h1>
      <div id="status" class="status">Connecting…</div>
    </div>

    <!-- Mode Selection -->
    <div class="card">
      <h2 class="card-title">Display Mode</h2>
      <label>
        <span class="label-text">What to show</span>
        <select id="mode-selector">
          <option value="parks">Park Wait Times</option>
          <option value="countdowns">Countdowns</option>
        </select>
      </label>
    </div>

    <!-- Device Settings -->
    <div class="card">
      <h2 class="card-title">Device</h2>
      <label>
        <span class="label-text">Device timezone</span>
        <select id="device_tz"></select>
      </label>
      <p class="helper">Used for countdowns.</p>
    </div>

    <!-- Parks Settings -->
    <div id="parks-settings">
      <div class="card">
        <h2 class="card-title">Resort</h2>
        <label>
          <span class="label-text">Select your destination</span>
          <select id="resort-selector">
            <option value="orlando">Walt Disney World (Florida)</option>
            <option value="california">Disneyland Resort (California)</option>
            <option value="tokyo">Tokyo Disney Resort (Japan)</option>
          </select>
        </label>
      </div>

      <div class="card">
        <h2 class="card-title">Trip Settings</h2>
        <label>
          <span class="label-text">Trip name</span>
          <input type="text" id="trip_name" placeholder="Tokyo Disney" maxlength="40">
        </label>
        <p class="helper">Optional. If blank, ParkPal will pick a default based on your selected park(s).</p>
        <label class="toggle-row">
          <span class="label-text">Show trip countdown</span>
          <div class="toggle">
            <input type="checkbox" id="trip_enabled">
            <span class="toggle-slider"></span>
          </div>
        </label>
        <label id="trip-date-row">
          <span class="label-text">Trip date</span>
          <input type="date" id="trip_date">
        </label>
        <label>
          <span class="label-text">Temperature units</span>
          <select id="units">
            <option value="metric">Celsius (°C)</option>
            <option value="imperial">Fahrenheit (°F)</option>
          </select>
        </label>
      </div>

      <div class="card">
        <h2 class="card-title">Parks to Display</h2>
        <div id="parks-grid" class="park-grid">
          <!-- Parks injected by JS -->
        </div>
        <p class="helper">Select which parks to cycle through. Pick at least one.</p>
        <div id="parks-accordions">
          <!-- Accordions injected by JS -->
        </div>
      </div>
    </div>

    <!-- Countdowns Settings -->
    <div id="countdowns-settings" class="hidden">
      <div class="card">
        <h2 class="card-title">Your Countdowns</h2>
        <div id="countdown-list">
          <!-- Countdown items injected by JS -->
        </div>
        <button type="button" class="add-countdown" id="add-countdown-btn">+ Add Countdown</button>
      </div>

      <div class="card">
        <h2 class="card-title">Display Options</h2>
        <label>
          <span class="label-text">Show mode</span>
          <select id="cd-show-mode">
            <option value="single">Single countdown</option>
            <option value="cycle">Cycle through all</option>
          </select>
        </label>
        <div id="cd-single-opts">
          <label>
            <span class="label-text">Primary countdown</span>
            <select id="cd-primary-id"></select>
          </label>
        </div>
        <div id="cd-cycle-opts" class="hidden">
          <label>
            <span class="label-text">Advance every N refreshes</span>
            <input type="number" id="cd-cycle-n" min="1" value="1">
          </label>
          <p class="helper">Use checkboxes above to include/exclude from cycle.</p>
        </div>
      </div>
    </div>
  </div>

  <!-- Action Bar -->
  <div class="action-bar">
    <button type="button" class="btn-secondary" id="btn-reload">Reload</button>
    <button type="button" class="btn-primary" id="btn-save">Save Changes</button>
  </div>

  <!-- Overlay -->
  <div class="overlay" id="overlay"></div>

  <!-- Ride Picker Modal -->
  <div class="modal" id="ride-picker">
    <div class="modal-header">
      <h2>Choose a Ride</h2>
      <button class="modal-close" id="close-picker">×</button>
    </div>
    <input type="text" class="search-input" id="ride-search" placeholder="Search rides…">
    <ul class="ride-list" id="ride-list"></ul>
  </div>

  <!-- Add Countdown Modal -->
  <div class="modal" id="add-cd-modal">
    <div class="modal-header">
      <h2>Add Countdown</h2>
      <button class="modal-close" id="close-add-cd">×</button>
    </div>
    <div class="modal-body">
      <p class="helper" style="margin-top:0">Choose a preset or create custom:</p>
      <div class="preset-grid">
        <button class="preset-btn" data-preset="christmas">🎄 Christmas</button>
        <button class="preset-btn" data-preset="halloween">🎃 Halloween</button>
        <button class="preset-btn" data-preset="newyear">🎆 New Year</button>
        <button class="preset-btn" data-preset="birthday">🎂 Birthday</button>
        <button class="preset-btn" data-preset="disney-wdw">🏰 Disney Trip (WDW)</button>
        <button class="preset-btn" data-preset="disney-tdr">🗼 Disney Trip (TDR)</button>
        <button class="preset-btn" data-preset="disney-dlr">🏰 Disney Trip (DLR)</button>
        <button class="preset-btn" data-preset="custom" style="grid-column: span 2">✨ Custom Countdown</button>
      </div>
    </div>
  </div>

  <!-- Edit Countdown Modal -->
  <div class="modal" id="edit-cd-modal">
    <div class="modal-header">
      <h2 id="edit-cd-title">Edit Countdown</h2>
      <button class="modal-close" id="close-edit-cd">×</button>
    </div>
    <div class="modal-body">
      <input type="hidden" id="cd-id">
      <label>
        <span class="label-text">Label Line 1</span>
        <input type="text" id="cd-label-1" placeholder="e.g., CHRISTMAS">
      </label>
      <label>
        <span class="label-text">Label Line 2 (optional)</span>
        <input type="text" id="cd-label-2" placeholder="e.g., COUNTDOWN">
      </label>
      <label>
        <span class="label-text">Label Line 3 (optional)</span>
        <input type="text" id="cd-label-3">
      </label>
      <label>
        <span class="label-text">Label Line 4 (optional)</span>
        <input type="text" id="cd-label-4">
      </label>
      <label>
        <span class="label-text">Repeat</span>
        <select id="cd-repeat">
          <option value="yearly">Every year</option>
          <option value="once">One-time only</option>
        </select>
      </label>
      <div id="cd-yearly-fields">
        <div class="row">
          <label>
            <span class="label-text">Month</span>
            <select id="cd-month">
              <option value="1">January</option>
              <option value="2">February</option>
              <option value="3">March</option>
              <option value="4">April</option>
              <option value="5">May</option>
              <option value="6">June</option>
              <option value="7">July</option>
              <option value="8">August</option>
              <option value="9">September</option>
              <option value="10">October</option>
              <option value="11">November</option>
              <option value="12">December</option>
            </select>
          </label>
          <label>
            <span class="label-text">Day</span>
            <input type="number" id="cd-day" min="1" max="31">
          </label>
        </div>
      </div>
      <div id="cd-once-fields" class="hidden">
        <label>
          <span class="label-text">Date</span>
          <input type="date" id="cd-once-date">
        </label>
      </div>
      <label>
        <span class="label-text">Birth year (for age display)</span>
        <input type="number" id="cd-birth-year" placeholder="Optional, e.g., 2016">
      </label>
      <label>
        <span class="label-text">Accent color</span>
        <select id="cd-accent">
          <option value="auto">Auto (red when ≤3 days)</option>
          <option value="black">Always black</option>
          <option value="red">Always red</option>
        </select>
      </label>
      <label>
        <span class="label-text">Icon</span>
        <select id="cd-icon">
          <option value="auto">Auto (based on event)</option>
          <option value="tree">🎄 Christmas Tree</option>
          <option value="reindeer">🦌 Reindeer</option>
          <option value="pumpkin">🎃 Pumpkin</option>
          <option value="ghost">👻 Ghost</option>
          <option value="cake">🎂 Birthday Cake</option>
          <option value="none">No icon</option>
        </select>
      </label>
    </div>
    <div class="modal-footer">
      <button type="button" id="save-cd-btn">Save Countdown</button>
    </div>
  </div>

<script>
// ==============================================
// ParkPal Configuration UI
// ==============================================

// Resort and Park definitions
const RESORTS = {
  orlando: {
    name: "Walt Disney World",
    abbr: "WDW",
    tz: "EST5EDT,M3.2.0/2,M11.1.0/2",
    defaultUnits: "imperial",
    parks: {
      6: { name: "Magic Kingdom", abbr: "MK" },
      5: { name: "EPCOT", abbr: "EP" },
      7: { name: "Hollywood Studios", abbr: "HS" },
      8: { name: "Animal Kingdom", abbr: "AK" }
    }
  },
  california: {
    name: "Disneyland Resort",
    abbr: "DLR",
    tz: "PST8PDT,M3.2.0/2,M11.1.0/2",
    defaultUnits: "imperial",
    parks: {
      16: { name: "Disneyland", abbr: "DL" },
      17: { name: "Disney California Adventure", abbr: "DCA" }
    }
  },
  tokyo: {
    name: "Tokyo Disney Resort",
    abbr: "TDR",
    tz: "JST-9",
    defaultUnits: "metric",
    parks: {
      274: { name: "Tokyo Disneyland", abbr: "TDL" },
      275: { name: "Tokyo DisneySea", abbr: "TDS" }
    }
  }
};

// State
let cfg = {};
let rideCache = {};
let currentPick = { parkId: null, slot: null };

// Popular/iconic ride suggestions used to pre-fill the first 5 slots when a park is first enabled.
// Matching is best-effort (by name tokens) against the live rides list from the Worker.
const POPULAR_RIDES = {
  // WDW
  6:  [ ['tron'], ['seven','dwarfs'], ['space','mountain'], ['haunted','mansion'], ['pirates'] ], // Magic Kingdom
  5:  [ ['guardians'], ['remy'], ['test','track'], ['soarin'], ['frozen'] ],                      // EPCOT
  7:  [ ['rise','resistance'], ['slinky'], ['runaway','railway'], ['smugglers'], ['tower'] ],     // Hollywood Studios
  8:  [ ['flight','passage'], ['navi'], ['kilimanjaro'], ['everest'], ['dinosaur'] ],             // Animal Kingdom

  // Disneyland Resort
  16: [ ['rise','resistance'], ['indiana'], ['space','mountain'], ['haunted','mansion'], ['pirates'] ], // Disneyland
  17: [ ['radiator'], ['mission','breakout'], ['incredicoaster'], ['web','slingers'], ['soarin'] ],     // DCA

  // Tokyo Disney Resort
  274:[ ['beauty','beast'], ['pooh'], ['monsters'], ['big','thunder'], ['splash'] ],              // Tokyo Disneyland
  275:[ ['journey','center'], ['soaring'], ['toy','story'], ['indiana'], ['tower'] ]              // Tokyo DisneySea
};

function normName(s) {
  if (!s) return '';
  return String(s)
    .replace(/[’‘]/g, "'")
    .replace(/[“”]/g, '"')
    .replace(/[–—]/g, '-')
    .replace(/[^a-zA-Z0-9 ]+/g, ' ')
    .replace(/\s+/g, ' ')
    .trim()
    .toLowerCase();
}

function findRideByTokens(rides, tokens) {
  const want = tokens.map(t => normName(t)).filter(Boolean);
  if (!want.length) return null;
  for (const ride of rides) {
    const n = normName(ride?.name);
    if (!n) continue;
    let ok = true;
    for (const t of want) {
      if (!n.includes(t)) { ok = false; break; }
    }
    if (ok) return ride;
  }
  return null;
}

async function maybeAutofillPopularRides(parkId) {
  const suggestions = POPULAR_RIDES[parkId];
  if (!suggestions) return;

  cfg.rides_by_park_ids = cfg.rides_by_park_ids || {};
  cfg.rides_by_park_labels = cfg.rides_by_park_labels || {};
  cfg.rides_by_park_ids[parkId] = cfg.rides_by_park_ids[parkId] || [0,0,0,0,0,0];
  cfg.rides_by_park_labels[parkId] = cfg.rides_by_park_labels[parkId] || ['','','','','',''];

  const ids = cfg.rides_by_park_ids[parkId];
  const labels = cfg.rides_by_park_labels[parkId];

  // Only auto-fill if the first 5 slots are all empty (don't override user choices).
  for (let i = 0; i < 5; i++) {
    if ((ids[i] || 0) !== 0 || (labels[i] || '').trim().length) return;
  }

  const rides = await fetchRides(parkId);
  if (!rides || !rides.length) return;

  const chosenIds = new Set(ids.filter(x => x > 0));
  let changed = false;

  for (let i = 0; i < 5 && i < suggestions.length; i++) {
    const ride = findRideByTokens(rides, suggestions[i]);
    if (!ride) continue;
    if (chosenIds.has(ride.id)) continue;
    ids[i] = ride.id;
    labels[i] = ride.name;
    chosenIds.add(ride.id);
    changed = true;
  }

  if (!changed) return;

  // If slots are currently rendered, update them live.
  for (let i = 0; i < 5; i++) {
    const nameEl = $(`slot-name-${parkId}-${i}`);
    if (!nameEl) continue;
    const label = labels[i] || '';
    nameEl.textContent = label || 'Not selected';
    nameEl.classList.toggle('empty', !label);
  }
}

// DOM helpers
const $ = id => document.getElementById(id);
const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];

function escapeHtml(v) {
  return String(v ?? '').replace(/[&<>"']/g, ch => ({
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#39;'
  }[ch]));
}

const DEVICE_TIMEZONES = [
  { label: 'Eastern (ET)', tz: 'EST5EDT,M3.2.0/2,M11.1.0/2' },
  { label: 'Central (CT)', tz: 'CST6CDT,M3.2.0/2,M11.1.0/2' },
  { label: 'Mountain (MT)', tz: 'MST7MDT,M3.2.0/2,M11.1.0/2' },
  { label: 'Arizona (no DST)', tz: 'MST7' },
  { label: 'Pacific (PT)', tz: 'PST8PDT,M3.2.0/2,M11.1.0/2' },
  { label: 'Alaska (AKT)', tz: 'AKST9AKDT,M3.2.0/2,M11.1.0/2' },
  { label: 'Hawaii (HST)', tz: 'HST10' },
  { label: 'UTC', tz: 'UTC0' }
];

function detectDefaultDeviceTz() {
  try {
    const iana = Intl.DateTimeFormat().resolvedOptions().timeZone || '';
    const map = {
      'America/New_York': DEVICE_TIMEZONES[0].tz,
      'America/Detroit': DEVICE_TIMEZONES[0].tz,
      'America/Indiana/Indianapolis': DEVICE_TIMEZONES[0].tz,
      'America/Chicago': DEVICE_TIMEZONES[1].tz,
      'America/Denver': DEVICE_TIMEZONES[2].tz,
      'America/Phoenix': DEVICE_TIMEZONES[3].tz,
      'America/Los_Angeles': DEVICE_TIMEZONES[4].tz,
      'America/Vancouver': DEVICE_TIMEZONES[4].tz,
      'America/Anchorage': DEVICE_TIMEZONES[5].tz,
      'Pacific/Honolulu': DEVICE_TIMEZONES[6].tz,
      'Etc/UTC': DEVICE_TIMEZONES[7].tz
    };
    return map[iana] || DEVICE_TIMEZONES[0].tz;
  } catch (_) {
    return DEVICE_TIMEZONES[0].tz;
  }
}

function populateDeviceTzOptions() {
  const sel = $('device_tz');
  sel.innerHTML = '';
  for (const z of DEVICE_TIMEZONES) {
    const opt = document.createElement('option');
    opt.value = z.tz;
    opt.textContent = z.label;
    sel.appendChild(opt);
  }
}

// ==============================================
// Load / Save
// ==============================================

async function loadConfig() {
  try {
    const res = await fetch('/api/config');
    if (!res.ok) throw new Error('Load failed');
    cfg = await res.json();
    
    // Ensure defaults
    cfg.mode = cfg.mode || 'parks';
    cfg.resort = cfg.resort || 'orlando';
    cfg.countdowns = cfg.countdowns || [];
    cfg.countdowns_settings = cfg.countdowns_settings || { show_mode: 'single', primary_id: '', cycle_every_n_refreshes: 1 };
    cfg.countdowns_tz = cfg.countdowns_tz || detectDefaultDeviceTz();
    
    $('status').textContent = 'Connected';
    $('status').classList.add('connected');
    
    populateUI();
  } catch (e) {
    console.error(e);
    $('status').textContent = 'Error';
  }
}

async function saveConfig() {
  const btn = $('btn-save');
  btn.disabled = true;
  btn.textContent = 'Saving…';
  
  try {
    gatherConfig();
    
    // Validate
    if (cfg.mode === 'parks') {
      const parks = cfg.parks_enabled || [];
      if (parks.length === 0) {
        alert('Please select at least one park.');
        throw new Error('validation');
      }
    }
    
    const res = await fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(cfg)
    });
    
    if (!res.ok) throw new Error('Save failed');
    
    // Trigger refresh
    await fetch('/api/refresh', { method: 'POST' });
    
    btn.textContent = 'Saved!';
    setTimeout(() => {
      btn.disabled = false;
      btn.textContent = 'Save Changes';
    }, 2000);
    
  } catch (e) {
    console.error(e);
    btn.disabled = false;
    btn.textContent = 'Save Changes';
    if (e.message !== 'validation') alert('Failed to save.');
  }
}

function gatherConfig() {
  cfg.mode = $('mode-selector').value;
  cfg.resort = $('resort-selector').value;
  cfg.trip_enabled = $('trip_enabled').checked;
  cfg.trip_date = $('trip_date').value || '';
  cfg.trip_name = ($('trip_name').value || '').trim();
  cfg.units = $('units').value;
  cfg.countdowns_tz = $('device_tz').value;
  
  // Set timezone based on resort
  const resort = RESORTS[cfg.resort];
  cfg.parks_tz = resort.tz;
  
  // Gather enabled parks
  cfg.parks_enabled = $$('.park-check.checked').map(el => parseInt(el.dataset.parkId));
  
  // Countdown settings
  cfg.countdowns_settings.show_mode = $('cd-show-mode').value;
  cfg.countdowns_settings.primary_id = $('cd-primary-id').value || (cfg.countdowns[0]?.id || '');
  cfg.countdowns_settings.cycle_every_n_refreshes = parseInt($('cd-cycle-n').value, 10) || 1;
}

// ==============================================
// UI Population
// ==============================================

function populateUI() {
  populateDeviceTzOptions();

  // Mode
  $('mode-selector').value = cfg.mode;
  updateModeVisibility();
  
  // Resort
  $('resort-selector').value = cfg.resort || 'orlando';
  
  // Trip settings
  $('trip_enabled').checked = !!cfg.trip_enabled;
  $('trip_date').value = (cfg.trip_date || '').slice(0, 10);
  $('trip_name').value = cfg.trip_name || '';
  $('units').value = cfg.units || RESORTS[cfg.resort || 'orlando'].defaultUnits;
  $('device_tz').value = cfg.countdowns_tz || detectDefaultDeviceTz();
  
  // Parks
  renderParksGrid();
  renderParksAccordions();
  updateTripNamePlaceholder();
  
  // Countdowns
  renderCountdownsList();
  renderCountdownSettings();
}

function inferDefaultTripName() {
  const resortKey = $('resort-selector')?.value || cfg.resort || 'orlando';
  const resort = RESORTS[resortKey] || RESORTS.orlando;

  // Prefer what's currently checked in the UI (live), else fall back to config.
  const checked = $$('.park-check.checked').map(el => parseInt(el.dataset.parkId, 10)).filter(n => Number.isFinite(n));
  const enabled = checked.length ? checked : (cfg.parks_enabled || []);
  const parkNames = enabled.map(pid => resort.parks?.[pid]?.name).filter(Boolean);

  if (parkNames.length === 1) return parkNames[0];
  if (resortKey === 'tokyo') return 'Tokyo Disney';
  if (resortKey === 'california') return 'Disneyland';
  return 'Disney World';
}

function updateTripNamePlaceholder() {
  const el = $('trip_name');
  if (!el) return;
  el.placeholder = inferDefaultTripName();
}

function updateModeVisibility() {
  const mode = $('mode-selector').value;
  $('parks-settings').classList.toggle('hidden', mode !== 'parks');
  $('countdowns-settings').classList.toggle('hidden', mode !== 'countdowns');
}

// ==============================================
// Parks UI
// ==============================================

function renderParksGrid() {
  const resort = RESORTS[cfg.resort || 'orlando'];
  const grid = $('parks-grid');
  grid.innerHTML = '';
  
  const enabledParks = cfg.parks_enabled || [];
  
  for (const [parkId, park] of Object.entries(resort.parks)) {
    const isChecked = enabledParks.includes(parseInt(parkId));
    const div = document.createElement('div');
    div.className = 'park-check' + (isChecked ? ' checked' : '');
    div.dataset.parkId = parkId;
    div.innerHTML = `
      <span class="checkmark"></span>
      <div>
        <div class="park-name">${park.name}</div>
        <div class="park-abbr">${park.abbr}</div>
      </div>
    `;
    div.addEventListener('click', () => {
      div.classList.toggle('checked');
      renderParksAccordions();
      updateTripNamePlaceholder();
    });
    grid.appendChild(div);
  }
}

async function renderParksAccordions() {
  const container = $('parks-accordions');
  container.innerHTML = '';
  
  const resort = RESORTS[cfg.resort || 'orlando'];
  const enabledParks = $$('.park-check.checked').map(el => parseInt(el.dataset.parkId));
  
  for (const parkId of enabledParks) {
    const park = resort.parks[parkId];
    if (!park) continue;
    
	    // Ensure ride data structures exist
	    cfg.rides_by_park_ids = cfg.rides_by_park_ids || {};
	    cfg.rides_by_park_labels = cfg.rides_by_park_labels || {};
	    cfg.rides_by_park_ids[parkId] = cfg.rides_by_park_ids[parkId] || [0,0,0,0,0,0];
	    cfg.rides_by_park_labels[parkId] = cfg.rides_by_park_labels[parkId] || ['','','','','',''];
	    // Best-effort: prefill first 5 slots with iconic rides for this park.
	    // Runs async and updates the slot labels when complete.
	    maybeAutofillPopularRides(parkId);
    
    const acc = document.createElement('div');
    acc.className = 'accordion open';
    acc.innerHTML = `
      <div class="accordion-header">
        <span>${park.name}</span>
        <span class="accordion-icon">▼</span>
      </div>
      <div class="accordion-body" id="acc-body-${parkId}"></div>
    `;
    
    acc.querySelector('.accordion-header').addEventListener('click', () => {
      acc.classList.toggle('open');
    });
    
    container.appendChild(acc);
    
    const body = $(`acc-body-${parkId}`);
    const ids = cfg.rides_by_park_ids[parkId];
    const labels = cfg.rides_by_park_labels[parkId];
    
	    for (let i = 0; i < 6; i++) {
	      const slot = document.createElement('div');
	      slot.className = 'ride-slot';
	      const label = labels[i] || '';
	      slot.innerHTML = `
	        <div class="slot-num">${i + 1}</div>
	        <div class="slot-name ${label ? '' : 'empty'}" id="slot-name-${parkId}-${i}">${label ? escapeHtml(label) : 'Not selected'}</div>
	        <button class="slot-btn" data-park="${parkId}" data-slot="${i}">Choose</button>
	      `;
      slot.querySelector('.slot-btn').addEventListener('click', (e) => {
        openRidePicker(parseInt(e.target.dataset.park), parseInt(e.target.dataset.slot));
      });
      body.appendChild(slot);
    }
  }
}

// ==============================================
// Ride Picker
// ==============================================

async function fetchRides(parkId) {
  if (rideCache[parkId]) return rideCache[parkId];
  
  try {
    const res = await fetch(`/api/rides?park=${parkId}`);
    const data = await res.json();
    const rides = data.rides || data.parks?.[0]?.rides || [];
    rideCache[parkId] = rides;
    return rides;
  } catch (e) {
    console.error('Failed to fetch rides:', e);
    return [];
  }
}

async function openRidePicker(parkId, slot) {
  currentPick = { parkId, slot };
  
  const rides = await fetchRides(parkId);
  const list = $('ride-list');
  list.innerHTML = '';
  
  for (const ride of rides) {
    const li = document.createElement('li');
    li.textContent = ride.name;
    li.dataset.id = ride.id;
    li.dataset.name = ride.name;
    li.addEventListener('click', () => selectRide(ride.id, ride.name));
    list.appendChild(li);
  }
  
  $('ride-search').value = '';
  filterRides();
  
  openModal('ride-picker');
}

function selectRide(id, name) {
  const { parkId, slot } = currentPick;
  
  cfg.rides_by_park_ids[parkId][slot] = id;
  cfg.rides_by_park_labels[parkId][slot] = name;
  
  const nameEl = $(`slot-name-${parkId}-${slot}`);
  nameEl.textContent = name;
  nameEl.classList.remove('empty');
  
  closeModal();
}

function filterRides() {
  const q = $('ride-search').value.toLowerCase().trim();
  $$('#ride-list li').forEach(li => {
    li.style.display = li.textContent.toLowerCase().includes(q) ? '' : 'none';
  });
}

// ==============================================
// Countdowns UI
// ==============================================

function renderCountdownsList() {
  const list = $('countdown-list');
  list.innerHTML = '';
  
  for (const cd of cfg.countdowns) {
    const labelPreview = (cd.label || []).filter(l => l).join(' ') || 'Untitled';
    
    const item = document.createElement('div');
    item.className = 'countdown-item';
    item.innerHTML = `
      <div class="cd-check ${cd.include_in_cycle !== false ? 'checked' : ''}" data-id="${cd.id}" title="Include in cycle"></div>
      <div class="cd-label">${escapeHtml(labelPreview)}</div>
      <div class="cd-actions">
        <button class="btn-edit" data-id="${cd.id}">Edit</button>
        <button class="btn-delete" data-id="${cd.id}">Delete</button>
      </div>
    `;
    
    item.querySelector('.cd-check').addEventListener('click', (e) => {
      e.target.classList.toggle('checked');
      const cdItem = cfg.countdowns.find(c => c.id === e.target.dataset.id);
      if (cdItem) cdItem.include_in_cycle = e.target.classList.contains('checked');
    });
    
    item.querySelector('.btn-edit').addEventListener('click', (e) => {
      openEditCountdown(e.target.dataset.id);
    });
    
    item.querySelector('.btn-delete').addEventListener('click', (e) => {
      deleteCountdown(e.target.dataset.id);
    });
    
    list.appendChild(item);
  }
}

function renderCountdownSettings() {
  $('cd-show-mode').value = cfg.countdowns_settings.show_mode || 'single';
  $('cd-cycle-n').value = cfg.countdowns_settings.cycle_every_n_refreshes || 1;
  
  updateCountdownModeVisibility();
  populatePrimaryDropdown();
}

function updateCountdownModeVisibility() {
  const mode = $('cd-show-mode').value;
  $('cd-single-opts').classList.toggle('hidden', mode !== 'single');
  $('cd-cycle-opts').classList.toggle('hidden', mode !== 'cycle');
}

function populatePrimaryDropdown() {
  const select = $('cd-primary-id');
  select.innerHTML = '';
  
  for (const cd of cfg.countdowns) {
    const label = (cd.label || []).filter(l => l).join(' ') || 'Untitled';
    const opt = document.createElement('option');
    opt.value = cd.id;
    opt.textContent = label;
    select.appendChild(opt);
  }
  
  select.value = cfg.countdowns_settings.primary_id || (cfg.countdowns[0]?.id || '');
}

// ==============================================
// Countdown Editor
// ==============================================

function openAddCountdown() {
  openModal('add-cd-modal');
}

function addCountdownFromPreset(preset) {
  closeModal();
  
  const id = `cd_${Date.now()}`;
  let template = { id, label: [], repeat: 'yearly', accent: 'auto', include_in_cycle: true, icon: 'auto' };
  
  switch (preset) {
    case 'christmas':
      template = { ...template, month: 12, day: 25, label: ['CHRISTMAS', 'COUNTDOWN'], icon: 'tree' };
      break;
    case 'halloween':
      template = { ...template, month: 10, day: 31, label: ['HALLOWEEN', 'COUNTDOWN'], icon: 'pumpkin' };
      break;
    case 'newyear':
      template = { ...template, month: 1, day: 1, label: ['NEW YEAR', 'COUNTDOWN'] };
      break;
    case 'birthday':
      template = { ...template, label: ["SOMEONE'S", 'BIRTHDAY', 'COUNTDOWN'], icon: 'cake' };
      break;
    case 'disney-wdw':
      template = { ...template, repeat: 'once', label: ['DISNEY WORLD', 'TRIP COUNTDOWN'] };
      break;
    case 'disney-tdr':
      template = { ...template, repeat: 'once', label: ['TOKYO DISNEY', 'TRIP COUNTDOWN'] };
      break;
    case 'disney-dlr':
      template = { ...template, repeat: 'once', label: ['DISNEYLAND', 'TRIP COUNTDOWN'] };
      break;
    case 'custom':
      template = { ...template, label: ['MY', 'COUNTDOWN'] };
      break;
  }
  
  cfg.countdowns.push(template);
  openEditCountdown(id);
}

function openEditCountdown(id) {
  const cd = cfg.countdowns.find(c => c.id === id);
  if (!cd) return;
  
  $('edit-cd-title').textContent = 'Edit Countdown';
  $('cd-id').value = cd.id;
  
  for (let i = 1; i <= 4; i++) {
    $(`cd-label-${i}`).value = (cd.label || [])[i - 1] || '';
  }
  
  $('cd-repeat').value = cd.repeat || 'yearly';
  $('cd-month').value = cd.month || 1;
  $('cd-day').value = cd.day || 1;
  
  if (cd.repeat === 'once' && cd.year && cd.month && cd.day) {
    $('cd-once-date').value = `${cd.year}-${String(cd.month).padStart(2,'0')}-${String(cd.day).padStart(2,'0')}`;
  } else {
    $('cd-once-date').value = '';
  }
  
  $('cd-birth-year').value = cd.birth_year || '';
  $('cd-accent').value = cd.accent || 'auto';
  $('cd-icon').value = cd.icon || 'auto';
  
  updateRepeatFields();
  openModal('edit-cd-modal');
}

function updateRepeatFields() {
  const repeat = $('cd-repeat').value;
  $('cd-yearly-fields').classList.toggle('hidden', repeat !== 'yearly');
  $('cd-once-fields').classList.toggle('hidden', repeat !== 'once');
}

function saveCountdown() {
  const id = $('cd-id').value;
  let cd = cfg.countdowns.find(c => c.id === id);
  
  if (!cd) {
    cd = { id };
    cfg.countdowns.push(cd);
  }
  
  cd.label = [
    $('cd-label-1').value.trim(),
    $('cd-label-2').value.trim(),
    $('cd-label-3').value.trim(),
    $('cd-label-4').value.trim()
  ];
  
  if (cd.label.every(l => !l)) {
    alert('Please enter at least one label line.');
    return;
  }
  
  cd.repeat = $('cd-repeat').value;
  
  if (cd.repeat === 'once') {
    const dateVal = $('cd-once-date').value;
    if (!dateVal) {
      alert('Please select a date.');
      return;
    }
    const [y, m, d] = dateVal.split('-').map(Number);
    cd.year = y;
    cd.month = m;
    cd.day = d;
  } else {
    cd.year = 0;
    cd.month = parseInt($('cd-month').value, 10);
    cd.day = parseInt($('cd-day').value, 10);
    
    if (!cd.month || !cd.day) {
      alert('Please select month and day.');
      return;
    }
  }
  
  cd.birth_year = parseInt($('cd-birth-year').value, 10) || 0;
  cd.accent = $('cd-accent').value;
  cd.icon = $('cd-icon').value;
  
  if (typeof cd.include_in_cycle === 'undefined') {
    cd.include_in_cycle = true;
  }
  
  closeModal();
  renderCountdownsList();
  populatePrimaryDropdown();
}

function deleteCountdown(id) {
  if (!confirm('Delete this countdown?')) return;
  
  cfg.countdowns = cfg.countdowns.filter(c => c.id !== id);
  
  if (cfg.countdowns_settings.primary_id === id) {
    cfg.countdowns_settings.primary_id = cfg.countdowns[0]?.id || '';
  }
  
  renderCountdownsList();
  populatePrimaryDropdown();
}

// ==============================================
// Modal Helpers
// ==============================================

function openModal(id) {
  $('overlay').classList.add('open');
  $(id).classList.add('open');
}

function closeModal() {
  $('overlay').classList.remove('open');
  $$('.modal.open').forEach(m => m.classList.remove('open'));
}

// ==============================================
// Event Listeners
// ==============================================

// Mode change
$('mode-selector').addEventListener('change', updateModeVisibility);

// Resort change
$('resort-selector').addEventListener('change', () => {
  const resort = RESORTS[$('resort-selector').value];
  
  // Reset parks selection for new resort
  cfg.parks_enabled = Object.keys(resort.parks).map(Number);
  cfg.resort = $('resort-selector').value;
  
  // Update default units
  $('units').value = resort.defaultUnits;
  
  // Clear ride cache for new resort
  rideCache = {};
  
  renderParksGrid();
  renderParksAccordions();
  updateTripNamePlaceholder();
});

// Countdown mode change
$('cd-show-mode').addEventListener('change', updateCountdownModeVisibility);

// Repeat field toggle
$('cd-repeat').addEventListener('change', updateRepeatFields);

// Ride search
$('ride-search').addEventListener('input', filterRides);

// Buttons
$('btn-reload').addEventListener('click', loadConfig);
$('btn-save').addEventListener('click', saveConfig);
$('add-countdown-btn').addEventListener('click', openAddCountdown);
$('save-cd-btn').addEventListener('click', saveCountdown);

// Close modals
$('overlay').addEventListener('click', closeModal);
$('close-picker').addEventListener('click', closeModal);
$('close-add-cd').addEventListener('click', closeModal);
$('close-edit-cd').addEventListener('click', closeModal);

// Preset buttons
$$('.preset-btn').forEach(btn => {
  btn.addEventListener('click', () => addCountdownFromPreset(btn.dataset.preset));
});

// ==============================================
// Initialize
// ==============================================

loadConfig();

</script>
</body>
</html>
)html";
