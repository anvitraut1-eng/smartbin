// service-worker.js — cache the app shell so the PWA launches offline.
//
// Strategy:
//   - on install: precache index.html, styles.css, app.js, manifest, icons
//   - on fetch for same-origin shell: cache-first
//   - on fetch for /api/*: network-first (bin data must be live; never cached)
//   - on activate: clean out old caches

const CACHE = 'smartbin-v1';
const SHELL = [
  './',
  'index.html',
  'styles.css',
  'app.js',
  'manifest.webmanifest',
  'icons/icon-192.png',
  'icons/icon-512.png',
  'icons/icon-maskable-512.png'
];

self.addEventListener('install', e => {
  e.waitUntil(
    caches.open(CACHE).then(c => c.addAll(SHELL)).then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k)))
    ).then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', e => {
  const url = new URL(e.request.url);
  // Never cache API responses.
  if (url.pathname.startsWith('/api/')) {
    e.respondWith(fetch(e.request).catch(() => new Response(
      JSON.stringify({ error: 'offline' }),
      { headers: { 'Content-Type': 'application/json' } }
    )));
    return;
  }
  // Same-origin shell: cache-first.
  if (url.origin === self.location.origin) {
    e.respondWith(
      caches.match(e.request).then(c => c || fetch(e.request).then(resp => {
        if (resp.ok) {
          const copy = resp.clone();
          caches.open(CACHE).then(cache => cache.put(e.request, copy));
        }
        return resp;
      }))
    );
  }
});
