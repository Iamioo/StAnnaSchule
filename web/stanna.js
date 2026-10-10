// St. Anna Schule - browser front end for stanna.wasm (built from src/*.c).
// Canvas, keyboard, touch controls (can be switched off), gamepad,
// WebAudio and sharing the high score.
"use strict";

const K = { UP: 1, DOWN: 2, LEFT: 4, RIGHT: 8, ACT: 16, RUN: 32, START: 64, ESC: 128, PHONE: 256 };
const ST_WIN = 5;
const W = 320, H = 200, TICK_MS = 1000 / 30;

const $ = (id) => document.getElementById(id);
const canvas = $("screen");
const ctx2d = canvas.getContext("2d");
const image = ctx2d.createImageData(W, H);

let wasm = null;
let keyKeys = 0, touchKeys = 0, padKeys = 0, latch = 0;
let lastState = -1;
let audioCtx = null, muted = false;

function store(key, value) {
  try {
    if (value === undefined) return localStorage.getItem(key);
    localStorage.setItem(key, value);
  } catch (e) { /* private mode: no storage */ }
  return null;
}

function fmtTime(sec) {
  const m = Math.floor(sec / 60), s = sec % 60;
  return m + ":" + String(s).padStart(2, "0");
}

// ---- keyboard ----
const keyMap = {
  ArrowUp: K.UP, KeyW: K.UP, ArrowDown: K.DOWN, KeyS: K.DOWN,
  ArrowLeft: K.LEFT, KeyA: K.LEFT, ArrowRight: K.RIGHT, KeyD: K.RIGHT,
  KeyE: K.ACT, Space: K.ACT, ShiftLeft: K.RUN, ShiftRight: K.RUN,
  KeyQ: K.PHONE, KeyF: K.PHONE, Enter: K.START, NumpadEnter: K.START, Escape: K.ESC,
};

addEventListener("keydown", (e) => {
  const k = keyMap[e.code];
  if (!k || e.target.tagName === "INPUT") return;
  e.preventDefault();
  keyKeys |= k;
  latch |= k;
  startAudio();
});
addEventListener("keyup", (e) => {
  const k = keyMap[e.code];
  if (k) keyKeys &= ~k;
});
addEventListener("blur", () => { keyKeys = 0; touchKeys = 0; });

// ---- touch controls ----
const pointers = new Map();

function touchBits(el, x, y) {
  if (el === "pad") {
    const r = $("pad").getBoundingClientRect();
    const dx = x - (r.left + r.width / 2), dy = y - (r.top + r.height / 2);
    const dead = r.width * 0.12;
    let k = 0;
    if (Math.abs(dx) > dead && Math.abs(dx) * 2 > Math.abs(dy)) k |= dx < 0 ? K.LEFT : K.RIGHT;
    if (Math.abs(dy) > dead && Math.abs(dy) * 2 > Math.abs(dx)) k |= dy < 0 ? K.UP : K.DOWN;
    return k;
  }
  return K[el] || 0;
}

function rebuildTouch() {
  let k = 0;
  for (const p of pointers.values()) k |= touchBits(p.el, p.x, p.y);
  touchKeys = k;
  latch |= k;
  document.querySelectorAll("[data-key]").forEach((b) => {
    b.classList.toggle("down", (k & (K[b.dataset.key] || 0)) !== 0);
  });
}

document.querySelectorAll(".tc").forEach((el) => {
  const name = el.dataset.key || el.id;
  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    pointers.set(e.pointerId, { el: name, x: e.clientX, y: e.clientY });
    startAudio();
    rebuildTouch();
  });
  el.addEventListener("pointermove", (e) => {
    const p = pointers.get(e.pointerId);
    if (!p) return;
    p.x = e.clientX;
    p.y = e.clientY;
    rebuildTouch();
  });
  const up = (e) => { pointers.delete(e.pointerId); rebuildTouch(); };
  el.addEventListener("pointerup", up);
  el.addEventListener("pointercancel", up);
});

// tapping the picture = Enter (start, continue)
canvas.addEventListener("pointerdown", () => { latch |= K.START; startAudio(); });

function setTouch(on) {
  document.body.classList.toggle("touch", on);
  $("btnTouch").textContent = "Handysteuerung: " + (on ? "AN" : "AUS");
  store("stanna_touch", on ? "1" : "0");
  if (wasm) wasm.touch_ui(on ? 1 : 0);
}
$("btnTouch").addEventListener("click", () => setTouch(!document.body.classList.contains("touch")));

// ---- gamepad (standard mapping) ----
function pollPad() {
  padKeys = 0;
  const pads = navigator.getGamepads ? navigator.getGamepads() : [];
  for (const p of pads) {
    if (!p) continue;
    const b = (i) => p.buttons[i] && p.buttons[i].pressed;
    if (p.axes[0] < -0.4 || b(14)) padKeys |= K.LEFT;
    if (p.axes[0] > 0.4 || b(15)) padKeys |= K.RIGHT;
    if (p.axes[1] < -0.4 || b(12)) padKeys |= K.UP;
    if (p.axes[1] > 0.4 || b(13)) padKeys |= K.DOWN;
    if (b(0)) padKeys |= K.ACT;
    if (b(1) || b(2) || b(7)) padKeys |= K.RUN;
    if (b(3)) padKeys |= K.PHONE;
    if (b(9)) padKeys |= K.START;
    if (b(8)) padKeys |= K.ESC;
  }
}

// ---- sound ----
function startAudio() {
  if (audioCtx || !wasm) {
    if (audioCtx && audioCtx.state === "suspended") audioCtx.resume();
    return;
  }
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC) return;
  audioCtx = new AC();
  wasm.audio_rate(audioCtx.sampleRate);
  const node = audioCtx.createScriptProcessor(2048, 0, 1);
  node.onaudioprocess = (ev) => {
    const out = ev.outputBuffer.getChannelData(0);
    if (muted || document.hidden) { out.fill(0); return; }
    const ptr = wasm.audio(out.length);
    const src = new Int16Array(wasm.memory.buffer, ptr, out.length);
    for (let i = 0; i < out.length; i++) out[i] = src[i] / 32768;
  };
  node.connect(audioCtx.destination);
  $("hint").classList.add("hidden");
}

function setMuted(m) {
  muted = m;
  $("btnSound").textContent = "Ton: " + (m ? "AUS" : "AN");
  store("stanna_muted", m ? "1" : "0");
}
$("btnSound").addEventListener("click", () => { setMuted(!muted); startAudio(); });

$("btnFull").addEventListener("click", () => {
  const el = $("stage");
  if (document.fullscreenElement) document.exitFullscreen();
  else if (el.requestFullscreen) el.requestFullscreen().catch(() => {});
  else if (el.webkitRequestFullscreen) el.webkitRequestFullscreen();
});

// ---- high score ----
function shareText(sec, stickers, deaths) {
  return "Ich bin in " + fmtTime(sec) + " aus der St. Anna Schule entkommen! " +
    "Sticker: " + stickers + "/18, " + deaths + "x erwischt. Schaffst du es schneller?";
}

function shareUrl(sec, stickers, deaths) {
  const u = new URL(location.href);
  u.search = "";
  u.hash = "";
  u.searchParams.set("zeit", sec);
  u.searchParams.set("sticker", stickers);
  u.searchParams.set("erwischt", deaths);
  return u.toString();
}

function onWin() {
  const sec = Math.floor(wasm.end_ticks() / 30);
  const stickers = wasm.stickers(), deaths = wasm.deaths();
  const best = parseInt(store("stanna_best") || "0", 10);
  const record = !best || sec < best;
  if (record) store("stanna_best", String(sec));
  showBest();
  const text = shareText(sec, stickers, deaths);
  const url = shareUrl(sec, stickers, deaths);
  $("winText").textContent = (record ? "Neue Bestzeit! " : "") + fmtTime(sec) +
    " – Sticker " + stickers + "/18 – " + deaths + "x erwischt";
  $("btnWa").href = "https://wa.me/?text=" + encodeURIComponent(text + " " + url);
  $("btnShare").onclick = async () => {
    if (navigator.share) {
      try { await navigator.share({ title: "St. Anna Schule", text, url }); } catch (e) { /* closed */ }
      return;
    }
    try {
      await navigator.clipboard.writeText(text + " " + url);
      $("btnShare").textContent = "Kopiert!";
    } catch (e) {
      prompt("Zum Teilen kopieren:", text + " " + url);
    }
  };
  $("btnShare").textContent = navigator.share ? "Highscore teilen" : "Link kopieren";
  $("win").classList.remove("hidden");
}
$("btnWinClose").addEventListener("click", () => $("win").classList.add("hidden"));

function showBest() {
  const best = parseInt(store("stanna_best") || "0", 10);
  $("best").textContent = best ? "Deine Bestzeit: " + fmtTime(best) : "Noch keine Bestzeit – entkomm zum ersten Mal!";
}

function showChallenge() {
  const p = new URLSearchParams(location.search);
  const sec = parseInt(p.get("zeit") || "", 10);
  if (!(sec > 0)) return;
  const st = parseInt(p.get("sticker") || "0", 10) || 0;
  const d = parseInt(p.get("erwischt") || "0", 10) || 0;
  $("challenge").textContent = "Herausforderung: Jemand ist in " + fmtTime(sec) + " entkommen (Sticker " +
    st + "/18, " + d + "x erwischt). Schaffst du es schneller?";
  $("challenge").classList.remove("hidden");
}

// ---- main loop ----
let acc = 0, last = 0;
function frame(now) {
  requestAnimationFrame(frame);
  if (!last) last = now;
  acc += Math.min(now - last, 250);
  last = now;
  if (acc < TICK_MS) return;
  pollPad();
  let steps = 0;
  while (acc >= TICK_MS && steps < 4) {
    if (!wasm.tick(keyKeys | touchKeys | padKeys | latch)) wasm.init((Math.random() * 4294967295) >>> 0);
    latch = 0;
    acc -= TICK_MS;
    steps++;
  }
  if (steps === 4) acc = 0;
  const st = wasm.state();
  if (st === ST_WIN && lastState !== ST_WIN) onWin();
  if (st !== ST_WIN) $("win").classList.add("hidden");
  lastState = st;
  const ptr = wasm.draw();
  image.data.set(new Uint8ClampedArray(wasm.memory.buffer, ptr, W * H * 4));
  ctx2d.putImageData(image, 0, 0);
}

async function boot() {
  showBest();
  showChallenge();
  const bytes = await (await fetch("stanna.wasm")).arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  wasm = instance.exports;
  wasm.init((Math.random() * 4294967295) >>> 0);
  const saved = store("stanna_touch");
  const coarse = matchMedia("(pointer: coarse)").matches || "ontouchstart" in window ||
    navigator.maxTouchPoints > 0;
  setTouch(saved === null ? coarse : saved === "1");
  setMuted(store("stanna_muted") === "1");
  $("loading").classList.add("hidden");
  requestAnimationFrame(frame);
}

boot().catch((e) => {
  $("loading").textContent = "Das Spiel konnte nicht geladen werden: " + e;
});
