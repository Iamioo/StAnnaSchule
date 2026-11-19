// Ersetzt das Spiel, solange die Seite offline sein soll.
// Wieder einschalten: in .github/workflows/pages.yml den Job "offline" löschen
// und beim Job "deploy" die Zeile "if: false" entfernen.
const html = `<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Offline</title>
<style>body{margin:0;min-height:100vh;display:grid;place-items:center;background:#111;color:#eee;font:18px system-ui,sans-serif;text-align:center}</style>
</head><body><div><h1>Offline</h1><p>Diese Seite ist gerade nicht erreichbar.</p></div></body></html>`;
export default {
  async fetch() {
    return new Response(html, { status: 503, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" } });
  },
};
