// St. Anna Schule - the gate in front of the browser version.
//
// A Cloudflare Worker: you sign in with Discord, and only accounts on the
// whitelist get the game. New accounts land in "wartet"; the admin accepts
// or blocks them on /admin. Everything else (index.html, stanna.wasm, the
// downloads) is served from ./public only to accepted accounts.
//
// Bindings (see wrangler.toml and the deploy workflow):
//   ASSETS                 the built site (public/)
//   USERS                  KV: "u:<discord id>" -> { id, name, avatar, status, first, last }
//   DISCORD_CLIENT_ID      secret
//   DISCORD_CLIENT_SECRET  secret
//   SESSION_SECRET         secret, a long random string
//   ADMIN_DISCORD_ID       secret, your Discord user id (several: comma separated)

const SESSION_DAYS = 30;
const enc = new TextEncoder();

// ---------- small helpers ----------

function b64url(bytes) {
  let s = "";
  for (const b of new Uint8Array(bytes)) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function fromB64url(str) {
  const s = atob(str.replace(/-/g, "+").replace(/_/g, "/"));
  return Uint8Array.from(s, (c) => c.charCodeAt(0));
}

function esc(s) {
  return String(s ?? "").replace(/[&<>"']/g, (c) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
}

function cookies(req) {
  const out = {};
  for (const part of (req.headers.get("Cookie") || "").split(";")) {
    const i = part.indexOf("=");
    if (i > 0) out[part.slice(0, i).trim()] = part.slice(i + 1).trim();
  }
  return out;
}

function setCookie(name, value, maxAge) {
  return `${name}=${value}; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=${maxAge}`;
}

async function hmac(secret, data) {
  const key = await crypto.subtle.importKey("raw", enc.encode(secret), { name: "HMAC", hash: "SHA-256" },
    false, ["sign", "verify"]);
  return b64url(await crypto.subtle.sign("HMAC", key, enc.encode(data)));
}

function isAdmin(env, id) {
  return String(env.ADMIN_DISCORD_ID || "").split(",").map((s) => s.trim()).filter(Boolean).includes(id);
}

// ---------- sessions: "<base64 json>.<hmac>" in a cookie ----------

async function makeSession(env, id) {
  const body = b64url(enc.encode(JSON.stringify({ id, exp: Date.now() + SESSION_DAYS * 864e5 })));
  return body + "." + (await hmac(env.SESSION_SECRET, body));
}

async function readSession(env, req) {
  const raw = cookies(req).stanna_session;
  if (!raw || !env.SESSION_SECRET) return null;
  const [body, sig] = raw.split(".");
  if (!body || !sig || (await hmac(env.SESSION_SECRET, body)) !== sig) return null;
  try {
    const s = JSON.parse(new TextDecoder().decode(fromB64url(body)));
    return s.exp > Date.now() ? s : null;
  } catch {
    return null;
  }
}

async function getUser(env, id) {
  const v = await env.USERS.get("u:" + id);
  return v ? JSON.parse(v) : null;
}

async function putUser(env, u) {
  await env.USERS.put("u:" + u.id, JSON.stringify(u));
}

// ---------- pages ----------

const STYLE = `
  :root { --bg:#14101c; --panel:#1f1a2b; --line:#3a3350; --text:#e8e4f0; --dim:#9a93ad;
    --red:#e03c3c; --yellow:#f0d058; --green:#58a848; --blurple:#5865f2; }
  * { box-sizing: border-box; }
  html, body { margin: 0; background: var(--bg); color: var(--text);
    font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif; }
  body { padding: 32px 16px; }
  main { max-width: 640px; margin: 0 auto; display: grid; gap: 18px; }
  h1 { margin: 0; font-size: 24px; letter-spacing: .5px; }
  h1 span { color: var(--red); }
  h2 { margin: 8px 0 0; font-size: 16px; color: var(--dim); text-transform: uppercase; letter-spacing: 1px; }
  p { margin: 0; color: var(--dim); }
  .card { background: var(--panel); border: 1px solid var(--line); border-radius: 10px; padding: 18px;
    display: grid; gap: 12px; }
  .btn { display: inline-flex; align-items: center; justify-content: center; gap: 8px; padding: 10px 16px;
    border-radius: 8px; border: 1px solid var(--line); background: var(--panel); color: var(--text);
    font: inherit; font-weight: 600; text-decoration: none; cursor: pointer; }
  .btn:focus-visible { outline: 2px solid var(--yellow); outline-offset: 2px; }
  .discord { background: var(--blurple); border-color: var(--blurple); color: #fff; justify-self: start; }
  .ok { border-color: var(--green); color: var(--green); }
  .no { border-color: var(--red); color: var(--red); }
  .row { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; padding: 10px 0;
    border-top: 1px solid var(--line); }
  .row:first-of-type { border-top: 0; }
  .who { flex: 1 1 200px; min-width: 0; display: flex; align-items: center; gap: 10px; }
  .who img, .who .ph { width: 36px; height: 36px; border-radius: 50%; background: var(--line); flex: none; }
  .who b { display: block; overflow-wrap: anywhere; }
  .who small { color: var(--dim); font-variant-numeric: tabular-nums; }
  .pill { font-size: 12px; padding: 2px 8px; border-radius: 99px; border: 1px solid var(--line); color: var(--dim); }
  .pill.wartet { border-color: var(--yellow); color: var(--yellow); }
  .pill.frei { border-color: var(--green); color: var(--green); }
  .pill.gesperrt { border-color: var(--red); color: var(--red); }
  form { margin: 0; display: flex; gap: 8px; flex-wrap: wrap; }
  .top { display: flex; justify-content: space-between; align-items: center; gap: 12px; flex-wrap: wrap; }
  a { color: var(--yellow); }
`;

function page(title, body, status = 200, headers = {}) {
  return new Response(`<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex, nofollow">
<title>${esc(title)}</title><style>${STYLE}</style></head><body><main>${body}</main></body></html>`,
    { status, headers: { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store", ...headers } });
}

const HEADER = `<h1>St. Anna Schule – <span>Herr Thüner kommt</span></h1>`;

function loginPage(msg = "") {
  return page("Anmelden – St. Anna Schule", `${HEADER}
    <div class="card">
      <p>Das Spiel ist nur für freigeschaltete Spieler. Melde dich mit Discord an –
      danach musst du freigegeben werden, bevor du spielen kannst.</p>
      ${msg ? `<p style="color:var(--red)">${esc(msg)}</p>` : ""}
      <a class="btn discord" href="/login">Mit Discord anmelden</a>
    </div>`);
}

function waitPage(u) {
  const blocked = u.status === "gesperrt";
  return page("Warten auf Freigabe – St. Anna Schule", `${HEADER}
    <div class="card">
      <p>Angemeldet als <b style="color:var(--text)">${esc(u.name)}</b>.</p>
      ${blocked
        ? `<p style="color:var(--red)">Dein Account ist für das Spiel gesperrt.</p>`
        : `<p>Deine Anfrage ist angekommen. Sobald du freigegeben bist, kannst du hier
           spielen – lade die Seite dann einfach neu.</p>`}
      <div><a class="btn" href="/">Neu laden</a> <a class="btn" href="/logout">Abmelden</a></div>
    </div>`);
}

function avatar(u) {
  return u.avatar
    ? `<img src="https://cdn.discordapp.com/avatars/${esc(u.id)}/${esc(u.avatar)}.png?size=64" alt="">`
    : `<span class="ph"></span>`;
}

function fmtDate(t) {
  return t ? new Date(t).toLocaleString("de-DE", { timeZone: "Europe/Berlin", dateStyle: "short", timeStyle: "short" }) : "";
}

function userRow(u, adminId) {
  const self = u.id === adminId;
  const btn = (action, label, cls) =>
    `<button class="btn ${cls}" name="action" value="${action}">${label}</button>`;
  let buttons = "";
  if (!self) {
    if (u.status !== "frei") buttons += btn("frei", "Annehmen", "ok");
    if (u.status !== "gesperrt") buttons += btn("gesperrt", u.status === "frei" ? "Sperren" : "Ablehnen", "no");
    if (u.status !== "wartet") buttons += btn("loeschen", "Entfernen", "");
  }
  return `<div class="row">
    <div class="who">${avatar(u)}<div><b>${esc(u.name)}</b>
      <small>${esc(u.id)} · zuletzt ${esc(fmtDate(u.last || u.first))}</small></div></div>
    <span class="pill ${esc(u.status)}">${self ? "admin" : esc(u.status)}</span>
    ${buttons ? `<form method="post" action="/admin"><input type="hidden" name="id" value="${esc(u.id)}">${buttons}</form>` : ""}
  </div>`;
}

async function adminPage(env, me) {
  const users = [];
  let cursor;
  do {
    const list = await env.USERS.list({ prefix: "u:", cursor });
    for (const k of list.keys) {
      const u = await getUser(env, k.name.slice(2));
      if (u) users.push(u);
    }
    cursor = list.list_complete ? null : list.cursor;
  } while (cursor);
  users.sort((a, b) => (b.last || b.first || 0) - (a.last || a.first || 0));
  const group = (st) => users.filter((u) => u.status === st);
  const section = (title, list, empty) => `<h2>${title} (${list.length})</h2>
    <div class="card">${list.length ? list.map((u) => userRow(u, me.id)).join("") : `<p>${empty}</p>`}</div>`;
  return page("Whitelist – St. Anna Schule", `<div class="top">${HEADER}
      <div><a class="btn" href="/">Zum Spiel</a> <a class="btn" href="/logout">Abmelden</a></div></div>
    ${section("Warten auf Freigabe", group("wartet"), "Niemand wartet gerade.")}
    ${section("Freigegeben", group("frei"), "Noch niemand freigegeben.")}
    ${section("Gesperrt", group("gesperrt"), "Niemand gesperrt.")}`);
}

// ---------- Discord OAuth ----------

function redirectUri(url) {
  return url.origin + "/callback";
}

async function login(env, url) {
  const state = b64url(crypto.getRandomValues(new Uint8Array(16)));
  const q = new URLSearchParams({
    client_id: env.DISCORD_CLIENT_ID, response_type: "code", scope: "identify",
    redirect_uri: redirectUri(url), state, prompt: "none",
  });
  return new Response(null, {
    status: 302,
    headers: { Location: "https://discord.com/oauth2/authorize?" + q, "Set-Cookie": setCookie("stanna_state", state, 600) },
  });
}

async function callback(env, req, url) {
  const code = url.searchParams.get("code");
  const state = url.searchParams.get("state");
  if (!code || !state || state !== cookies(req).stanna_state)
    return loginPage("Die Anmeldung ist abgelaufen oder wurde abgebrochen. Versuch es nochmal.");
  const tok = await fetch("https://discord.com/api/oauth2/token", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams({
      client_id: env.DISCORD_CLIENT_ID, client_secret: env.DISCORD_CLIENT_SECRET,
      grant_type: "authorization_code", code, redirect_uri: redirectUri(url),
    }),
  });
  if (!tok.ok) return loginPage("Discord hat die Anmeldung nicht bestätigt. Versuch es nochmal.");
  const { access_token } = await tok.json();
  const me = await fetch("https://discord.com/api/users/@me", { headers: { Authorization: "Bearer " + access_token } });
  if (!me.ok) return loginPage("Dein Discord-Profil konnte nicht gelesen werden.");
  const d = await me.json();
  const now = Date.now();
  const u = (await getUser(env, d.id)) || { id: d.id, status: "wartet", first: now };
  u.name = d.global_name || d.username;
  u.avatar = d.avatar || null;
  u.last = now;
  if (isAdmin(env, d.id)) u.status = "frei";
  await putUser(env, u);
  const h = new Headers({ Location: "/" });
  h.append("Set-Cookie", setCookie("stanna_session", await makeSession(env, d.id), SESSION_DAYS * 86400));
  h.append("Set-Cookie", setCookie("stanna_state", "", 0));
  return new Response(null, { status: 302, headers: h });
}

// ---------- the router ----------

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    const path = url.pathname;
    if (!env.DISCORD_CLIENT_ID || !env.DISCORD_CLIENT_SECRET || !env.SESSION_SECRET)
      return page("Noch nicht eingerichtet", `${HEADER}<div class="card"><p>Der Server ist noch nicht
        fertig eingerichtet (Discord-Daten fehlen). Siehe README, Abschnitt „Anmeldung mit Discord“.</p></div>`, 503);

    if (path === "/login") return login(env, url);
    if (path === "/callback") return callback(env, req, url);
    if (path === "/logout")
      return new Response(null, { status: 302, headers: { Location: "/", "Set-Cookie": setCookie("stanna_session", "", 0) } });

    const s = await readSession(env, req);
    const u = s && (await getUser(env, s.id));
    if (!u) {
      if (path === "/" || path === "/index.html") return loginPage();
      return new Response("Bitte zuerst anmelden.", { status: 401 });
    }

    if (path === "/admin") {
      if (!isAdmin(env, u.id)) return new Response("Nur für den Admin.", { status: 403 });
      if (req.method === "POST") {
        // only from our own page
        if (req.headers.get("Origin") !== url.origin) return new Response("Ungültige Anfrage.", { status: 403 });
        const f = await req.formData();
        const id = String(f.get("id") || "");
        const action = String(f.get("action") || "");
        const t = await getUser(env, id);
        if (t && !isAdmin(env, id)) {
          if (action === "loeschen") await env.USERS.delete("u:" + id);
          else if (action === "frei" || action === "gesperrt") {
            t.status = action;
            await putUser(env, t);
          }
        }
        return new Response(null, { status: 303, headers: { Location: "/admin" } });
      }
      return adminPage(env, u);
    }

    if (u.status !== "frei") {
      if (path === "/" || path === "/index.html") return waitPage(u);
      return new Response("Noch nicht freigegeben.", { status: 403 });
    }

    // accepted: the game and its files
    const res = await env.ASSETS.fetch(req);
    const out = new Response(res.body, res);
    out.headers.set("Cache-Control", "private, no-cache");
    return out;
  },
};
