// app.js — Smart Bin PWA logic. Vanilla JS, no framework, no build step.
//
// State: a list of bins in localStorage ("smartbin.bins"), each:
//   { id, name, host, ip, lastState }
// The app polls every bin's /api/state in parallel, renders cards, and lets
// the user calibrate / view history / reboot each bin.

const STORAGE_KEY = 'smartbin.bins';
const POLL_FOREGROUND = 3000;
const POLL_BACKGROUND = 15000;

let bins = loadBins();
let pollTimer = null;
let activeMenuBinId = null;   // bin currently shown in the context menu
let activeCalibBinId = null;  // bin currently in the calibration modal
let activeHistoryBinId = null;
let deferredPrompt = null;    // beforeinstallprompt event

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------
function loadBins() {
  try { return JSON.parse(localStorage.getItem(STORAGE_KEY)) || []; }
  catch { return []; }
}
function saveBins() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(bins));
}
function getBin(id) { return bins.find(b => b.id === id); }

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------
function httpBase(bin) {
  // Prefer mDNS host; fall back to stored IP if host fails (set on add).
  return `http://${bin.host}`;
}

async function fetchWithTimeout(url, ms = 2500) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), ms);
  try {
    const r = await fetch(url, { signal: ctrl.signal, cache: 'no-store' });
    return r.ok ? await r.json() : null;
  } catch { return null; }
  finally { clearTimeout(t); }
}

async function fetchState(bin) {
  const data = await fetchWithTimeout(`${httpBase(bin)}/api/state`);
  if (data) { bin.lastState = data; bin.lastSeen = Date.now(); }
  return data;
}

async function postJSON(bin, path, body) {
  try {
    const r = await fetch(`${httpBase(bin)}${path}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : undefined,
    });
    return r.ok ? await r.json() : null;
  } catch { return null; }
}

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------
async function pollAll() {
  if (!bins.length) { renderEmpty(true); return; }
  await Promise.all(bins.map(fetchState));
  render();
  updateGlobalStatus();
}

function startPolling() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = setInterval(pollAll, document.hidden ? POLL_BACKGROUND : POLL_FOREGROUND);
}

document.addEventListener('visibilitychange', () => {
  if (!document.hidden) { pollAll(); startPolling(); }
  else { startPolling(); }
});

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
function renderEmpty(show) {
  document.getElementById('emptyState').style.display = show ? 'block' : 'none';
  document.getElementById('binList').style.display = show ? 'none' : 'grid';
}

function levelClass(pct) {
  if (pct >= 80) return 'level-red';
  if (pct >= 50) return 'level-amber';
  return 'level-green';
}

function relativeTime(unixSec) {
  if (!unixSec) return 'never';
  const diff = Math.floor(Date.now() / 1000) - unixSec;
  if (diff < 0) return 'just now';
  if (diff < 60) return `${diff}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  return `${Math.floor(diff / 86400)}d ago`;
}

function render() {
  const list = document.getElementById('binList');
  if (!bins.length) { renderEmpty(true); return; }
  renderEmpty(false);

  list.innerHTML = bins.map(b => {
    const s = b.lastState;
    const online = !!s && (Date.now() - (b.lastSeen || 0) < 20000);
    const pct = s ? Math.round(s.fill_pct) : 0;
    const out = s && s.sensor_out_of_range;
    const cls = levelClass(pct);
    const emptied = s ? relativeTime(s.last_emptied) : '—';
    return `
      <div class="bin-card ${online ? '' : 'offline'}" data-id="${b.id}">
        <div class="bin-head">
          <div>
            <div class="name">${escapeHtml(b.name)}</div>
            <div class="meta">${escapeHtml(b.host)} · ${online ? '<span style="color:var(--green)">online</span>' : '<span style="color:var(--red)">offline</span>'}</div>
          </div>
          <button class="menu-btn" data-action="menu" data-id="${b.id}">⋯</button>
        </div>
        <div class="fill-row">
          <span class="fill-pct">${online ? pct : '—'}</span>
          <span class="fill-unit">${online ? '%' : ''}</span>
          ${out ? '<span class="badge warn">sensor blocked</span>' : ''}
          ${!online ? '<span class="badge">offline</span>' : ''}
          ${online && !s.calibrated ? '<span class="badge warn">needs calibration</span>' : ''}
        </div>
        <div class="fill-bar"><div class="${cls}" style="width:${online ? pct : 0}%"></div></div>
        <div class="bin-foot">
          <span>Last emptied: ${online ? emptied : '—'}</span>
          ${online && s.distance_cm ? `<span>${s.distance_cm.toFixed(1)} cm</span>` : ''}
        </div>
      </div>`;
  }).join('');
}

function updateGlobalStatus() {
  const dot = document.getElementById('globalStatus');
  const anyOnline = bins.some(b => b.lastState && (Date.now() - (b.lastSeen || 0) < 20000));
  dot.className = 'dot ' + (anyOnline ? 'online' : (bins.length ? 'offline' : 'offline'));
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

// ---------------------------------------------------------------------------
// Toast
// ---------------------------------------------------------------------------
let toastTimer = null;
function toast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.add('hidden'), 2200);
}

// ---------------------------------------------------------------------------
// Modals
// ---------------------------------------------------------------------------
function openModal(id) { document.getElementById(id).classList.remove('hidden'); }
function closeModal(id) { document.getElementById(id).classList.add('hidden'); }

document.querySelectorAll('[data-close]').forEach(btn => {
  btn.addEventListener('click', () => closeModal(btn.dataset.close));
});

// --- Add bin ---
document.getElementById('addBinBtn').addEventListener('click', () => {
  document.getElementById('addName').value = '';
  document.getElementById('addHost').value = '';
  document.getElementById('testResult').textContent = '';
  openModal('addBinModal');
});

document.getElementById('testBtn').addEventListener('click', async () => {
  const host = document.getElementById('addHost').value.trim();
  const res = document.getElementById('testResult');
  if (!host) { res.textContent = 'enter a host'; return; }
  res.textContent = 'testing…';
  const data = await fetchWithTimeout(`http://${host}/api/state`, 2500);
  res.innerHTML = data
    ? `<span style="color:var(--green)">✓ connected${data.calibrated ? '' : ' (needs calibration)'}</span>`
    : `<span style="color:var(--red)">✗ no response</span>`;
});

document.getElementById('saveBinBtn').addEventListener('click', async () => {
  const name = document.getElementById('addName').value.trim() || 'Bin';
  const host = document.getElementById('addHost').value.trim();
  if (!host) { toast('Enter a host'); return; }
  const data = await fetchWithTimeout(`http://${host}/api/info`, 2500);
  const id = (data && data.id) ? data.id : 'bin-' + Date.now();
  // Dedupe on id: update existing instead of adding a duplicate.
  const existing = bins.find(b => b.id === id);
  if (existing) { existing.name = name; existing.host = host; }
  else { bins.push({ id, name, host, ip: '', lastState: null }); }
  saveBins();
  closeModal('addBinModal');
  pollAll();
});

// --- Context menu ---
document.getElementById('binList').addEventListener('click', e => {
  const btn = e.target.closest('[data-action]');
  if (!btn) return;
  const id = btn.dataset.id;
  if (btn.dataset.action === 'menu') openMenu(id);
});

function openMenu(id) {
  const b = getBin(id); if (!b) return;
  activeMenuBinId = id;
  document.getElementById('menuName').textContent = b.name;
  openModal('menuModal');
}

document.querySelectorAll('[data-menu]').forEach(btn => {
  btn.addEventListener('click', () => {
    const action = btn.dataset.menu;
    const id = activeMenuBinId;
    closeModal('menuModal');
    if (action === 'calibrate') openCalib(id);
    else if (action === 'history') openHistory(id);
    else if (action === 'rename') { const n = prompt('New name', getBin(id).name); if (n) { getBin(id).name = n; saveBins(); render(); } }
    else if (action === 'reboot') { postJSON(getBin(id), '/api/reboot'); toast('Rebooting…'); }
    else if (action === 'remove') { bins = bins.filter(b => b.id !== id); saveBins(); render(); renderEmpty(!bins.length); }
  });
});

// --- Calibration ---
async function openCalib(id) {
  activeCalibBinId = id;
  const b = getBin(id);
  document.getElementById('calibName').textContent = b.name;
  const cur = document.getElementById('calCurrent');
  const s = b.lastState || {};
  cur.textContent = `Current: empty=${s.empty_cm ? s.empty_cm.toFixed(1) + 'cm' : '—'}, full=${s.full_cm ? s.full_cm.toFixed(1) + 'cm' : '—'}`;
  document.getElementById('manualEmpty').value = '';
  document.getElementById('manualFull').value = '';
  openModal('calibModal');
}

document.getElementById('calEmpty').addEventListener('click', async () => {
  const r = await postJSON(getBin(activeCalibBinId), '/api/calibrate/empty');
  toast(r && r.ok ? `Empty set: ${r.empty_cm}cm` : 'Failed');
  await pollAll();
});
document.getElementById('calFull').addEventListener('click', async () => {
  const r = await postJSON(getBin(activeCalibBinId), '/api/calibrate/full');
  toast(r && r.ok ? `Full set: ${r.full_cm}cm` : 'Failed');
  await pollAll();
});
document.getElementById('manualApply').addEventListener('click', async () => {
  const e = parseFloat(document.getElementById('manualEmpty').value);
  const f = parseFloat(document.getElementById('manualFull').value);
  const body = {};
  if (!isNaN(e)) body.empty_cm = e;
  if (!isNaN(f)) body.full_cm = f;
  const r = await postJSON(getBin(activeCalibBinId), '/api/calibrate', body);
  toast(r && r.ok ? 'Saved' : 'Failed');
  await pollAll();
});

// --- History ---
async function openHistory(id) {
  activeHistoryBinId = id;
  const b = getBin(id);
  document.getElementById('historyName').textContent = b.name;
  const list = document.getElementById('historyList');
  list.innerHTML = '<div class="muted">Loading…</div>';
  openModal('historyModal');
  const data = await fetchWithTimeout(`${httpBase(b)}/api/history?limit=100`, 3000);
  if (!data || !data.events) { list.innerHTML = '<div class="muted">No history</div>'; return; }
  if (!data.events.length) { list.innerHTML = '<div class="muted">No emptied events yet</div>'; return; }
  list.innerHTML = data.events.slice().reverse().map(ev => `
    <div class="history-item">
      <span>🗑️ Emptied</span>
      <span class="muted">${new Date(ev.t * 1000).toLocaleString()}<br>${relativeTime(ev.t)}</span>
    </div>`).join('');
}

document.getElementById('exportHistory').addEventListener('click', async () => {
  const b = getBin(activeHistoryBinId);
  const data = await fetchWithTimeout(`${httpBase(b)}/api/history?limit=1000`, 4000);
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `${b.name}-history.json`;
  a.click();
  URL.revokeObjectURL(a.href);
});

// ---------------------------------------------------------------------------
// Install prompt
// ---------------------------------------------------------------------------
window.addEventListener('beforeinstallprompt', e => {
  e.preventDefault();
  deferredPrompt = e;
  document.getElementById('installBanner').classList.remove('hidden');
});

document.getElementById('installYes').addEventListener('click', async () => {
  document.getElementById('installBanner').classList.add('hidden');
  if (deferredPrompt) { deferredPrompt.prompt(); await deferredPrompt.userChoice; deferredPrompt = null; }
});
document.getElementById('installNo').addEventListener('click', () => {
  document.getElementById('installBanner').classList.add('hidden');
});

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------
if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('service-worker.js').catch(() => {});
}

render();
pollAll();
startPolling();
