# Deploying the PWA to GitHub Pages

## Option 1: Manual upload (simplest for first-time deploy)

1. **Create a new GitHub repository**
   - Go to https://github.com/new
   - Name it `smartbin` (or anything you like)
   - Public or private (both work with Pages)
   - Don't initialize with README (we already have one)

2. **Push your code**
   ```bash
   cd C:/Users/anvit/smartbin
   git init
   git add .
   git commit -m "Initial commit: Smart Bin firmware + PWA"
   git branch -M main
   git remote add origin https://github.com/<your-username>/smartbin.git
   git push -u origin main
   ```

3. **Create and push a gh-pages branch with only the PWA**
   ```bash
   # Create an orphan branch (no history, clean slate)
   git checkout --orphan gh-pages
   
   # Remove everything except pwa/
   git rm -rf .
   git reset
   
   # Move pwa/ contents to root (GitHub Pages serves from /)
   mv pwa/* .
   mv pwa/.* . 2>/dev/null || true
   rmdir pwa
   
   # Commit and push
   git add .
   git commit -m "Deploy PWA to GitHub Pages"
   git push origin gh-pages
   ```

4. **Enable GitHub Pages**
   - Go to your repo → Settings → Pages
   - Source: Deploy from branch → `gh-pages` → `/ (root)` → Save
   - Wait ~1 min, then visit: `https://<your-username>.github.io/smartbin/`

5. **Return to main branch**
   ```bash
   git checkout main
   ```

---

## Option 2: GitHub Actions (auto-deploy on every push)

If you want the PWA to auto-update whenever you push changes to `main`:

1. Keep both firmware and PWA in `main` (don't do the orphan branch above)

2. Create `.github/workflows/deploy-pwa.yml`:
   ```yaml
   name: Deploy PWA to GitHub Pages
   
   on:
     push:
       branches: [main]
       paths:
         - 'pwa/**'
   
   permissions:
     contents: read
     pages: write
     id-token: write
   
   jobs:
     deploy:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4
         - uses: actions/configure-pages@v4
         - uses: actions/upload-pages-artifact@v3
           with:
             path: pwa/
         - uses: actions/deploy-pages@v4
   ```

3. Push to `main`:
   ```bash
   git add .github/workflows/deploy-pwa.yml
   git commit -m "Add GitHub Actions auto-deploy"
   git push
   ```

4. Enable GitHub Pages (Settings → Pages → Source: **GitHub Actions**)

The PWA deploys automatically on every push that touches `pwa/`.

---

## Option 3: Local server (testing only, no install)

If you just want to test locally before deploying:

```bash
cd C:/Users/anvit/smartbin/pwa
python -m http.server 8080
# Open http://localhost:8080
```

**Note:** PWA install (Add to Home Screen) **requires HTTPS**. Local HTTP works for testing the UI, but you won't get the install prompt until it's served over HTTPS (GitHub Pages gives you that for free).

---

## Verify the deployment

Once GitHub Pages is live:

1. Open `https://<your-username>.github.io/smartbin/` on your phone (same WiFi as the ESP32)
2. Tap **+ Add bin**
3. Enter:
   - Name: `Kitchen`
   - Host: `smartbin-kitchen.local` (or the IP of your ESP32)
4. Tap **Test** → should show ✓ connected
5. Tap **Save**
6. The bin card appears with live fill %

---

## Troubleshooting

**PWA doesn't load / shows 404:**
- Check GitHub Pages is enabled and set to the `gh-pages` branch (or Actions)
- Wait 1–2 minutes after pushing; GitHub Pages isn't instant
- Check the repo is public (or you have GitHub Pro for private Pages)

**"Add to Home Screen" doesn't appear:**
- PWA must be served over HTTPS (GitHub Pages does this automatically)
- On **iOS (Safari)**: no automatic prompt; user must tap Share → "Add to Home Screen" manually
- On **Android (Chrome)**: the install banner should appear after the first bin connects successfully

**Bin shows "offline" in the PWA:**
- Check your phone is on the same WiFi network as the ESP32
- Try the ESP32's IP address instead of mDNS (`smartbin-kitchen.local` doesn't work on all Android networks)
- Open `http://smartbin-kitchen.local/api/state` in your phone's browser to confirm it resolves

**Icons don't show up:**
- The PNGs are already in `pwa/icons/` and referenced in `manifest.webmanifest` — they should work out of the box
- Clear your browser cache if you deployed, then updated the icons later
