# Smart Bin PWA

Progressive Web App for the Smart Bin firmware. Vanilla JS, no build step, no
dependencies. Designed to be served from GitHub Pages (or any static HTTPS
host).

## Files

- `index.html` — single-page app shell
- `manifest.webmanifest` — PWA install metadata
- `service-worker.js` — offline cache of the app shell
- `app.js` — all logic (bin list, polling, modals, install prompt)
- `styles.css` — system font, dark/light via `prefers-color-scheme`
- `icons/` — 192/512/maskable-512 PNGs

## Run locally

The PWA needs to be served over HTTP (not `file://`) for `fetch` and service
worker to work. Any static file server works:

```bash
cd pwa
python -m http.server 8080
# open http://localhost:8080
```

## Deploy to GitHub Pages

1. Push `pwa/` to a `gh-pages` branch of a GitHub repo
2. In repo settings, set Pages source to the `gh-pages` branch root
3. Visit `https://<user>.github.io/<repo>/` on your phone

The service worker precaches the app shell, so subsequent loads work offline
once the PWA is installed.

## Install on iPhone

Open the deployed URL in Safari, tap the share icon, then "Add to Home
Screen". (Safari on iOS is more restrictive than Chrome — note that the
`beforeinstallprompt` event is Chrome-only; on iOS the user has to use the
share sheet manually.)

## Install on Android

Open the deployed URL in Chrome, accept the install banner (or use Chrome
menu → "Add to Home Screen").

## Configuring bins

The first time you open the PWA, tap **+ Add bin** and enter:

- **Friendly name** — anything (e.g. "Kitchen")
- **Host** — the mDNS name of your ESP32 (e.g. `smartbin-kitchen.local`) or
  its IP address as a fallback

The "Test" button calls `/api/state` and shows ✓ / ✗ before saving.

The bin list is stored in `localStorage` under `smartbin.bins` — clearing
site data will require re-adding bins.

## Notes on mDNS

mDNS (`*.local`) only resolves inside the local network. If the phone is on
cellular, the PWA cannot reach the bins. The bin list still appears (cached)
but shows as "offline".
