// Offline test of worker.js: fake KV, fake assets, fake Discord.
import worker from "./worker.js";
const kv = new Map();
const env = {
  DISCORD_CLIENT_ID: "123", DISCORD_CLIENT_SECRET: "sec", SESSION_SECRET: "x".repeat(40), ADMIN_DISCORD_ID: "1",
  USERS: {
    get: async (k) => kv.get(k) ?? null, put: async (k, v) => kv.set(k, v), delete: async (k) => kv.delete(k),
    list: async ({ prefix }) => ({ keys: [...kv.keys()].filter((k) => k.startsWith(prefix)).map((name) => ({ name })), list_complete: true }),
  },
  ASSETS: { fetch: async (r) => new Response("GAME " + new URL(r.url).pathname) },
};
let who = { id: "1", username: "ioannis" };
globalThis.fetch = async (u) => String(u).includes("token")
  ? new Response(JSON.stringify({ access_token: "t" }))
  : new Response(JSON.stringify({ ...who, avatar: null }));
const B = "https://stanna.example.workers.dev";
const call = (p, o = {}) => worker.fetch(new Request(B + p, o), env);
const cookieOf = (r) => (r.headers.getSetCookie?.() || [r.headers.get("Set-Cookie")]).map((c) => c.split(";")[0]).join("; ");
let ok = 0, bad = 0;
const t = (c, m) => { c ? ok++ : (bad++, console.log("FAIL", m)); };

async function signIn(user) {
  who = user;
  const l = await call("/login");
  const state = new URL(l.headers.get("Location")).searchParams.get("state");
  t(l.headers.get("Location").includes("redirect_uri=https%3A%2F%2Fstanna.example.workers.dev%2Fcallback"), "redirect uri");
  const c = await call(`/callback?code=abc&state=${state}`, { headers: { Cookie: cookieOf(l) } });
  t(c.status === 302, "callback redirects");
  return cookieOf(c).split("; ").find((x) => x.startsWith("stanna_session="));
}

t((await (await call("/")).text()).includes("Mit Discord anmelden"), "login page");
t((await call("/stanna.wasm")).status === 401, "wasm locked");
t((await call("/callback?code=a&state=zzz")).status === 200, "bad state shows login");
const admin = await signIn({ id: "1", username: "ioannis" });
t((await (await call("/", { headers: { Cookie: admin } })).text()) === "GAME /", "admin plays");
const guest = await signIn({ id: "2", username: "lukas<script>" });
t((await (await call("/", { headers: { Cookie: guest } })).text()).includes("Anfrage ist angekommen"), "guest waits");
t((await call("/stanna.wasm", { headers: { Cookie: guest } })).status === 403, "guest no wasm");
t((await call("/admin", { headers: { Cookie: guest } })).status === 403, "guest no admin");
const html = await (await call("/admin", { headers: { Cookie: admin } })).text();
t(html.includes("lukas&lt;script&gt;") && !html.includes("lukas<script>"), "escaped name");
const form = new URLSearchParams({ id: "2", action: "frei" });
t((await call("/admin", { method: "POST", body: form, headers: { Cookie: admin, "Content-Type": "application/x-www-form-urlencoded" } })).status === 403, "post without origin refused");
t((await call("/admin", { method: "POST", body: form, headers: { Cookie: admin, Origin: B, "Content-Type": "application/x-www-form-urlencoded" } })).status === 303, "approve");
t((await (await call("/stanna.wasm", { headers: { Cookie: guest } })).text()) === "GAME /stanna.wasm", "guest plays after approval");
await call("/admin", { method: "POST", body: new URLSearchParams({ id: "2", action: "gesperrt" }), headers: { Cookie: admin, Origin: B, "Content-Type": "application/x-www-form-urlencoded" } });
t((await (await call("/", { headers: { Cookie: guest } })).text()).includes("gesperrt"), "blocked at once");
await call("/admin", { method: "POST", body: new URLSearchParams({ id: "1", action: "gesperrt" }), headers: { Cookie: admin, Origin: B, "Content-Type": "application/x-www-form-urlencoded" } });
t(JSON.parse(kv.get("u:1")).status === "frei", "admin cannot lock himself out");
t((await call("/", { headers: { Cookie: guest.replace(/.$/, "A") } })).status === 200 && true, "tampered cookie -> login");
t((await (await call("/", { headers: { Cookie: guest.slice(0, -3) + "abc" } })).text()).includes("Mit Discord anmelden"), "bad signature rejected");
console.log(`${ok} ok, ${bad} failed`);
process.exit(bad ? 1 : 0);
