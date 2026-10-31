// Collector dashboard served at http://<board IP>/
// Captured JPEGs are kept in the browser (IndexedDB) so they survive a page reload,
// and are downloaded as ZIP files named "<class>.<number>.jpg" (Edge Impulse reads the label from the name).
#pragma once

static const char COLLECTOR_HTML[] = R"rawliteral(
<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO 데이터 수집</title>
<style>
  :root { --bg:#eef1f0; --panel:#fff; --fg:#17201d; --sub:#5e6b66; --line:#d5dcd9; --pcb:#1f6f54;
          --pcb-soft:#dcebe4; --bad:#c8412f; --warn:#b7791f; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#0f1513; --panel:#18201d; --fg:#e4ebe8; --sub:#93a39d; --line:#2a3531; --pcb:#4fbf95;
            --pcb-soft:#1d3a2f; --bad:#ef6a57; --warn:#e0a84a; }
  }
  * { box-sizing:border-box; }
  body { margin:0; padding:20px 16px; background:var(--bg); color:var(--fg);
         font-family:"Malgun Gothic",system-ui,sans-serif; font-size:15px; }
  .wrap { max-width:1180px; margin:0 auto; display:flex; flex-direction:column; gap:14px; }
  header { display:flex; flex-wrap:wrap; align-items:baseline; gap:6px 14px; }
  h1 { font-size:20px; margin:0; }
  .sub, .mono { color:var(--sub); font-size:12.5px; font-family:ui-monospace,Consolas,monospace; font-variant-numeric:tabular-nums; }
  .layout { display:grid; grid-template-columns:minmax(0,1fr) minmax(0,1.15fr); gap:14px; align-items:start; }
  @media (max-width:860px) { .layout { grid-template-columns:minmax(0,1fr); } }
  .col { display:flex; flex-direction:column; gap:14px; min-width:0; }
  .panel { background:var(--panel); border:1px solid var(--line); border-radius:12px; padding:14px;
           display:flex; flex-direction:column; gap:12px; min-width:0; }
  .label { font-size:12px; letter-spacing:.06em; text-transform:uppercase; color:var(--sub); font-family:ui-monospace,Consolas,monospace;
           display:flex; justify-content:space-between; gap:8px; }
  .frame { position:relative; background:#000; border-radius:8px; overflow:hidden; aspect-ratio:4/3; display:flex; align-items:center; justify-content:center; }
  .frame img { width:100%; height:100%; object-fit:contain; display:block; }
  .frame .flash { position:absolute; inset:0; background:#fff; opacity:0; pointer-events:none; transition:opacity .25s; }
  .frame .flash.on { opacity:.55; transition:none; }
  .frame .badge { position:absolute; left:10px; top:10px; background:rgba(0,0,0,.6); color:#fff; padding:4px 10px; border-radius:20px;
                  font-size:13px; font-weight:700; }
  .frame .count { position:absolute; inset:0; display:none; align-items:center; justify-content:center; color:#fff;
                  font-size:96px; font-weight:800; text-shadow:0 2px 12px rgba(0,0,0,.6); }
  .msg { color:#fff; font-size:14px; padding:16px; text-align:center; }
  .row { display:flex; flex-wrap:wrap; align-items:center; gap:8px; }
  .row > .grow { flex:1; }
  button, select, input { font:inherit; color:var(--fg); }
  button { font-weight:600; padding:9px 14px; border-radius:9px; border:1px solid var(--line); background:var(--bg); cursor:pointer; }
  button:disabled { opacity:.45; cursor:default; }
  button.primary { background:var(--pcb); border-color:var(--pcb); color:var(--panel); }
  button.danger { color:var(--bad); }
  button.small { padding:5px 10px; font-size:13px; }
  .big { font-size:17px; padding:13px 18px; }
  select, input[type=text], input[type=number] { padding:8px 10px; border-radius:9px; border:1px solid var(--line); background:var(--bg); min-width:0; }
  input[type=number] { width:84px; }
  input[type=range] { accent-color:var(--pcb); }
  .field { display:flex; flex-direction:column; gap:4px; font-size:12.5px; color:var(--sub); }
  .settings { display:grid; grid-template-columns:repeat(auto-fit,minmax(140px,1fr)); gap:10px; }
  .check { display:flex; align-items:center; gap:6px; font-size:14px; color:var(--fg); }
  .progress { height:6px; background:var(--line); border-radius:3px; overflow:hidden; }
  .progress > div { height:100%; width:0; background:var(--pcb); transition:width .15s; }
  .hint { font-size:12.5px; color:var(--sub); line-height:1.6; }
  kbd { font-family:ui-monospace,Consolas,monospace; font-size:11.5px; border:1px solid var(--line); border-bottom-width:2px;
        border-radius:4px; padding:0 5px; background:var(--bg); }
  button.primary kbd, button.danger kbd { background:transparent; color:inherit; border-color:currentColor; opacity:.75; }

  /* classes */
  .classes { display:flex; flex-direction:column; gap:6px; }
  .cls { display:grid; grid-template-columns:auto minmax(0,1fr) auto auto auto; align-items:center; gap:8px;
         padding:8px 10px; border:1px solid var(--line); border-radius:9px; cursor:pointer; }
  .cls.active { border-color:var(--pcb); background:var(--pcb-soft); }
  .cls .key { font-family:ui-monospace,Consolas,monospace; font-size:12px; color:var(--sub); width:18px; text-align:center; }
  .cls .name { font-weight:700; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
  .cls .n { font-family:ui-monospace,Consolas,monospace; font-variant-numeric:tabular-nums; font-size:13px; min-width:44px; text-align:right; }
  .bar { grid-column:2 / -1; height:4px; background:var(--line); border-radius:2px; overflow:hidden; }
  .bar > div { height:100%; background:var(--pcb); }
  .empty { color:var(--sub); font-size:13.5px; padding:6px 2px; }

  /* gallery */
  .tabs { display:flex; gap:6px; overflow-x:auto; padding-bottom:2px; }
  .tabs button { white-space:nowrap; }
  .tabs button.on { background:var(--fg); color:var(--panel); border-color:var(--fg); }
  .gallery { display:grid; grid-template-columns:repeat(auto-fill,minmax(92px,1fr)); gap:6px; max-height:560px; overflow-y:auto; align-content:start; }
  .thumb { position:relative; aspect-ratio:1; border-radius:7px; overflow:hidden; background:#000; cursor:zoom-in; border:2px solid transparent; }
  .thumb img { width:100%; height:100%; object-fit:cover; display:block; }
  .thumb.sel { border-color:var(--bad); }
  .thumb .tag { position:absolute; left:4px; bottom:4px; background:rgba(0,0,0,.6); color:#fff; font-size:11px; padding:1px 6px; border-radius:10px;
                max-width:calc(100% - 8px); overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
  .thumb .box { position:absolute; right:4px; top:4px; width:22px; height:22px; border-radius:6px; background:rgba(0,0,0,.45);
                border:2px solid #fff; cursor:pointer; color:#fff; font-size:13px; line-height:18px; text-align:center; }
  .thumb.sel .box { background:var(--bad); border-color:var(--bad); }

  /* viewer */
  .viewer { position:fixed; inset:0; background:rgba(0,0,0,.88); display:none; flex-direction:column; align-items:center; justify-content:center; gap:12px; padding:16px; z-index:10; }
  .viewer.open { display:flex; }
  .viewer img { max-width:min(92vw,720px); max-height:70vh; image-rendering:auto; border-radius:8px; background:#000; }
  .viewer .info { color:#ddd; font-size:13px; font-family:ui-monospace,Consolas,monospace; text-align:center; }
  .viewer .row { justify-content:center; }
  .viewer button, .viewer select { background:#222; color:#eee; border-color:#444; }
  .viewer button.danger { color:#ff8a78; }
  .toast { position:fixed; left:50%; bottom:20px; transform:translateX(-50%); background:var(--fg); color:var(--panel); padding:9px 16px;
           border-radius:9px; font-size:14px; opacity:0; transition:opacity .2s; pointer-events:none; z-index:20; max-width:calc(100% - 32px); }
  .toast.on { opacity:1; }
  .err { color:var(--bad); }
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>Edge Impulse 이미지 수집</h1>
    <span class="sub" id="meta">보드에 연결 중...</span>
  </header>

  <div class="layout">
    <div class="col">
      <section class="panel">
        <div class="label"><span>카메라</span><span id="camInfo"></span></div>
        <div class="frame" id="frame">
          <div class="msg">스트림 연결 중...</div>
        </div>
        <div class="row">
          <button type="button" class="primary big grow" id="shot">찍기 <kbd>Space</kbd></button>
          <button type="button" class="big" id="undo" title="방금 찍은 사진 삭제">되돌리기 <kbd>Z</kbd></button>
          <button type="button" class="big" id="toggle">일시정지</button>
        </div>
      </section>

      <section class="panel">
        <div class="label"><span>연속 촬영</span><span id="burstState"></span></div>
        <div class="row">
          <label class="field">장수<input type="number" id="burstCount" min="1" max="500" value="20"></label>
          <label class="field">간격 (ms)<input type="number" id="burstGap" min="0" max="10000" step="50" value="300"></label>
          <label class="field">시작 전 대기
            <select id="burstDelay"><option value="0">바로</option><option value="3">3초</option><option value="5">5초</option><option value="10">10초</option></select>
          </label>
          <button type="button" class="primary grow" id="burst" style="align-self:flex-end">연속 촬영 시작 <kbd>B</kbd></button>
        </div>
        <div class="progress"><div id="burstBar"></div></div>
        <div class="hint">연속 촬영 중에는 물체를 조금씩 돌리거나 위치·거리를 바꿔 주세요. 비슷한 사진만 많으면 모델이 쉽게 과적합됩니다.</div>
      </section>

      <section class="panel">
        <div class="label"><span>카메라 설정</span></div>
        <div class="settings">
          <label class="field">해상도
            <select id="framesize">
              <option value="96x96">96x96</option>
              <option value="160x120">160x120</option>
              <option value="240x240">240x240 (추천)</option>
              <option value="320x240">320x240</option>
              <option value="320x320">320x320</option>
              <option value="640x480">640x480</option>
            </select>
          </label>
          <label class="field">JPEG 품질 <span id="qualityVal"></span><input type="range" id="quality" min="4" max="40" step="1"></label>
          <label class="field">밝기 <span id="brightnessVal"></span><input type="range" id="brightness" min="-2" max="2" step="1"></label>
          <label class="field">대비 <span id="contrastVal"></span><input type="range" id="contrast" min="-2" max="2" step="1"></label>
          <label class="check"><input type="checkbox" id="vflip">상하 반전</label>
          <label class="check"><input type="checkbox" id="hmirror">좌우 반전</label>
        </div>
        <div class="hint">JPEG 품질은 숫자가 <b>낮을수록</b> 화질이 좋고 파일이 큽니다. 학습할 때와 추론할 때 같은 설정을 쓰는 게 좋습니다.</div>
      </section>
    </div>

    <div class="col">
      <section class="panel">
        <div class="label"><span>클래스 (라벨)</span><span id="total"></span></div>
        <form class="row" id="addForm">
          <input type="text" id="newClass" class="grow" placeholder="새 클래스 이름 (예: cup, phone, background)" maxlength="40" autocomplete="off">
          <button type="submit" class="primary">추가</button>
        </form>
        <div class="classes" id="classes"></div>
        <div class="row">
          <button type="button" class="grow" id="zipAll">전체 ZIP 다운로드 (클래스별 폴더)</button>
        </div>
        <div class="hint">클래스를 누르거나 숫자키 <kbd>1</kbd>~<kbd>9</kbd>로 고른 뒤 찍으면 그 클래스로 저장됩니다.
          파일 이름은 <span class="mono">클래스.번호.jpg</span> 형식이라 Edge Impulse 업로드 시 "Infer from filename"으로 라벨이 자동 지정됩니다.
          배경만 있는 <b>background</b> 클래스도 함께 모으면 정확도가 좋아집니다.</div>
      </section>

      <section class="panel">
        <div class="label"><span>수집한 사진</span><span id="storage"></span></div>
        <div class="tabs" id="tabs"></div>
        <div class="row">
          <button type="button" class="small" id="selAll">모두 선택</button>
          <button type="button" class="small" id="selNone">선택 해제</button>
          <span class="mono grow" id="selInfo"></span>
          <select id="moveTo" class="small" style="padding:5px 8px"></select>
          <button type="button" class="small" id="moveSel">이동</button>
          <button type="button" class="small danger" id="delSel">선택 삭제</button>
        </div>
        <div class="gallery" id="gallery"></div>
      </section>
    </div>
  </div>
</div>

<div class="viewer" id="viewer">
  <img id="vImg" alt="">
  <div class="info" id="vInfo"></div>
  <div class="row">
    <button type="button" id="vPrev">← 이전</button>
    <button type="button" id="vNext">다음 →</button>
    <select id="vMove"></select>
    <button type="button" class="danger" id="vDel">삭제 <kbd>Del</kbd></button>
    <button type="button" id="vClose">닫기 <kbd>Esc</kbd></button>
  </div>
</div>
<div class="toast" id="toast"></div>

<script>
const $ = id => document.getElementById(id);
const BOARD = location.hostname;

// ---------- storage (IndexedDB) ----------
let db;
function openDb() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open('xiao-ei-collector', 1);
    req.onupgradeneeded = () => {
      const d = req.result;
      d.createObjectStore('classes', { keyPath: 'name' });
      d.createObjectStore('images', { keyPath: 'id', autoIncrement: true }).createIndex('label', 'label');
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}
function tx(store, mode, fn) {
  return new Promise((resolve, reject) => {
    const t = db.transaction(store, mode);
    const result = fn(t.objectStore(store));
    t.oncomplete = () => resolve(result && 'result' in result ? result.result : undefined);
    t.onerror = () => reject(t.error);
  });
}
const getAll = store => tx(store, 'readonly', s => s.getAll());

// ---------- state ----------
let classes = [];      // [{name, created}]
let images = [];       // [{id, label, blob, ts, w, h}] newest last
let active = null;     // class used for new captures
let filter = '*';      // gallery tab
const selected = new Set();
const urls = new Map(); // id -> object URL
let lastIds = [];      // for undo

function urlOf(img) {
  if (!urls.has(img.id)) urls.set(img.id, URL.createObjectURL(img.blob));
  return urls.get(img.id);
}
function countOf(name) { return images.filter(i => i.label === name).length; }
function visible() { return filter === '*' ? images : images.filter(i => i.label === filter); }
function cleanName(s) { return s.trim().replace(/[.\/\\:*?"<>|]/g, '_').replace(/\s+/g, '_'); }

let toastTimer;
function toast(text, bad) {
  const t = $('toast');
  t.textContent = text;
  t.style.background = bad ? 'var(--bad)' : '';
  t.classList.add('on');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('on'), 1800);
}

// ---------- classes ----------
async function addClass(raw) {
  const name = cleanName(raw);
  if (!name) return;
  if (classes.some(c => c.name === name)) { setActive(name); return; }
  const c = { name, created: Date.now() };
  await tx('classes', 'readwrite', s => s.put(c));
  classes.push(c);
  setActive(name);
}
async function removeClass(name) {
  const n = countOf(name);
  if (n && !confirm(`"${name}" 클래스와 사진 ${n}장을 삭제할까요?`)) return;
  await deleteImages(images.filter(i => i.label === name).map(i => i.id));
  await tx('classes', 'readwrite', s => s.delete(name));
  classes = classes.filter(c => c.name !== name);
  if (active === name) active = classes[0] ? classes[0].name : null;
  if (filter === name) filter = '*';
  renderAll();
}
function setActive(name) {
  active = name;
  try { localStorage.setItem('ei-active', name); } catch (e) {}
  renderClasses();
  updateBadge();
}

function renderClasses() {
  const box = $('classes');
  const max = Math.max(1, ...classes.map(c => countOf(c.name)));
  box.replaceChildren();
  if (!classes.length) {
    box.innerHTML = '<div class="empty">아직 클래스가 없습니다. 위에서 먼저 추가하세요.</div>';
  }
  classes.forEach((c, i) => {
    const n = countOf(c.name);
    const el = document.createElement('div');
    el.className = 'cls' + (c.name === active ? ' active' : '');
    el.innerHTML = `<span class="key">${i < 9 ? i + 1 : ''}</span><span class="name"></span><span class="n">${n}장</span>
      <button type="button" class="small" data-act="zip" ${n ? '' : 'disabled'}>ZIP</button>
      <button type="button" class="small danger" data-act="del" title="클래스 삭제">✕</button>
      <div class="bar"><div style="width:${n / max * 100}%"></div></div>`;
    el.querySelector('.name').textContent = c.name;
    el.onclick = e => {
      const act = e.target.dataset && e.target.dataset.act;
      if (act === 'zip') downloadZip(c.name);
      else if (act === 'del') removeClass(c.name);
      else setActive(c.name);
    };
    box.appendChild(el);
  });
  $('total').textContent = `총 ${images.length}장`;
  $('zipAll').disabled = !images.length;
  $('shot').disabled = $('burst').disabled = !active && !bursting;

  const opts = classes.map(c => `<option></option>`).join('');
  for (const sel of [$('moveTo'), $('vMove')]) {
    const keep = sel.value;
    sel.innerHTML = '<option value="">클래스 이동…</option>' + opts;
    classes.forEach((c, i) => { const o = sel.options[i + 1]; o.value = c.name; o.textContent = '→ ' + c.name; });
    sel.value = keep && classes.some(c => c.name === keep) ? keep : '';
  }
}

// ---------- gallery ----------
function renderTabs() {
  const tabs = $('tabs');
  tabs.replaceChildren();
  const list = [['*', `전체 ${images.length}`], ...classes.map(c => [c.name, `${c.name} ${countOf(c.name)}`])];
  for (const [key, text] of list) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'small' + (filter === key ? ' on' : '');
    b.textContent = text;
    b.onclick = () => { filter = key; selected.clear(); renderTabs(); renderGallery(); };
    tabs.appendChild(b);
  }
}
function thumbEl(img) {
  const el = document.createElement('div');
  el.className = 'thumb' + (selected.has(img.id) ? ' sel' : '');
  el.dataset.id = img.id;
  el.innerHTML = `<img loading="lazy" alt=""><span class="tag"></span><span class="box">${selected.has(img.id) ? '✓' : ''}</span>`;
  el.querySelector('img').src = urlOf(img);
  el.querySelector('.tag').textContent = img.label;
  return el;
}
function renderGallery() {
  const g = $('gallery');
  const list = visible().slice().reverse();  // newest first
  g.replaceChildren(...list.map(thumbEl));
  if (!list.length) g.innerHTML = '<div class="empty">사진이 없습니다.</div>';
  renderSel();
}
function renderSel() {
  $('selInfo').textContent = selected.size ? `${selected.size}장 선택됨` : '';
  $('delSel').disabled = $('moveSel').disabled = !selected.size;
}
$('gallery').onclick = e => {
  const t = e.target.closest('.thumb');
  if (!t) return;
  const id = Number(t.dataset.id);
  if (e.target.classList.contains('box') || e.shiftKey || e.ctrlKey || e.metaKey) {
    selected.has(id) ? selected.delete(id) : selected.add(id);
    t.classList.toggle('sel', selected.has(id));
    t.querySelector('.box').textContent = selected.has(id) ? '✓' : '';
    renderSel();
  } else {
    openViewer(id);
  }
};
$('selAll').onclick = () => { visible().forEach(i => selected.add(i.id)); renderGallery(); };
$('selNone').onclick = () => { selected.clear(); renderGallery(); };
$('delSel').onclick = async () => {
  if (!confirm(`선택한 사진 ${selected.size}장을 삭제할까요?`)) return;
  await deleteImages([...selected]);
  selected.clear();
  renderAll();
};
$('moveSel').onclick = async () => {
  const to = $('moveTo').value;
  if (!to) { toast('이동할 클래스를 먼저 고르세요', true); return; }
  await relabel([...selected], to);
  toast(`${selected.size}장을 ${to}(으)로 옮겼습니다`);
  selected.clear();
  renderAll();
};

async function deleteImages(ids) {
  if (!ids.length) return;
  await tx('images', 'readwrite', s => ids.forEach(id => s.delete(id)));
  const gone = new Set(ids);
  images = images.filter(i => !gone.has(i.id));
  ids.forEach(id => { if (urls.has(id)) { URL.revokeObjectURL(urls.get(id)); urls.delete(id); } selected.delete(id); });
  lastIds = lastIds.filter(id => !gone.has(id));
}
async function relabel(ids, label) {
  const set = new Set(ids);
  const changed = images.filter(i => set.has(i.id));
  changed.forEach(i => { i.label = label; });
  await tx('images', 'readwrite', s => changed.forEach(i => s.put(i)));
}

// ---------- viewer ----------
let vId = null;
function openViewer(id) { vId = id; $('viewer').classList.add('open'); showViewer(); }
function closeViewer() { vId = null; $('viewer').classList.remove('open'); }
function showViewer() {
  const list = visible().slice().reverse();
  const idx = list.findIndex(i => i.id === vId);
  if (idx < 0) { closeViewer(); return; }
  const img = list[idx];
  $('vImg').src = urlOf(img);
  $('vInfo').textContent = `${img.label} · ${img.w}x${img.h} · ${(img.blob.size / 1024).toFixed(1)} KB · ${new Date(img.ts).toLocaleTimeString()} · ${idx + 1} / ${list.length}`;
  $('vPrev').disabled = idx === 0;
  $('vNext').disabled = idx === list.length - 1;
  $('vMove').value = '';
}
function stepViewer(d) {
  const list = visible().slice().reverse();
  const idx = list.findIndex(i => i.id === vId);
  const next = list[idx + d];
  if (next) { vId = next.id; showViewer(); }
}
async function deleteInViewer() {
  const list = visible().slice().reverse();
  const idx = list.findIndex(i => i.id === vId);
  const next = list[idx + 1] || list[idx - 1];
  await deleteImages([vId]);
  renderAll();
  if (next) { vId = next.id; showViewer(); } else closeViewer();
}
$('vPrev').onclick = () => stepViewer(-1);
$('vNext').onclick = () => stepViewer(1);
$('vDel').onclick = deleteInViewer;
$('vClose').onclick = closeViewer;
$('viewer').onclick = e => { if (e.target === $('viewer')) closeViewer(); };
$('vMove').onchange = async () => {
  const to = $('vMove').value;
  if (!to) return;
  await relabel([vId], to);
  toast(`${to}(으)로 옮겼습니다`);
  renderAll();
  showViewer();
};

// ---------- capture ----------
async function captureOne() {
  if (!active) { toast('클래스를 먼저 추가/선택하세요', true); return false; }
  const label = active;
  const r = await fetch('/capture?t=' + Date.now(), { cache: 'no-store' });
  if (!r.ok) throw new Error('capture ' + r.status);
  const blob = await r.blob();
  const img = { label, blob, ts: Date.now(), w: Number(r.headers.get('X-Width')) || 0, h: Number(r.headers.get('X-Height')) || 0 };
  img.id = await tx('images', 'readwrite', s => s.add(img));
  images.push(img);
  lastIds.push(img.id);
  if (lastIds.length > 50) lastIds.shift();

  const f = $('flash');
  if (f) { f.classList.add('on'); requestAnimationFrame(() => requestAnimationFrame(() => f.classList.remove('on'))); }
  // cheap incremental update instead of re-rendering every thumbnail
  if (filter === '*' || filter === label) {
    const g = $('gallery');
    if (g.querySelector('.empty')) g.replaceChildren();
    g.prepend(thumbEl(img));
  }
  renderClasses();
  renderTabs();
  updateBadge();
  return true;
}
$('shot').onclick = async () => {
  try { await captureOne(); } catch (e) { toast('촬영 실패: ' + e.message, true); }
};
$('undo').onclick = async () => {
  const id = lastIds.pop();
  if (id === undefined) { toast('되돌릴 사진이 없습니다'); return; }
  await deleteImages([id]);
  renderAll();
  toast('방금 찍은 사진을 삭제했습니다');
};

let bursting = false;
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function burst() {
  if (bursting) { bursting = false; return; }
  if (!active) { toast('클래스를 먼저 추가/선택하세요', true); return; }
  const total = Math.max(1, Math.min(500, Number($('burstCount').value) || 1));
  const gap = Math.max(0, Number($('burstGap').value) || 0);
  const delay = Number($('burstDelay').value);
  bursting = true;
  $('burst').textContent = '중지';
  $('burst').classList.remove('primary');
  $('burst').classList.add('danger');

  const count = $('countdown');
  for (let s = delay; s > 0 && bursting; s--) {
    if (count) { count.textContent = s; count.style.display = 'flex'; }
    $('burstState').textContent = `${s}초 후 시작`;
    await sleep(1000);
  }
  if (count) count.style.display = 'none';

  let done = 0, failed = 0;
  while (bursting && done < total) {
    const t0 = performance.now();
    try { if (await captureOne()) done++; else break; }
    catch (e) { if (++failed >= 5) { toast('연속 촬영 중 오류가 계속 나서 멈췄습니다', true); break; } }
    $('burstBar').style.width = (done / total * 100) + '%';
    $('burstState').textContent = `${done} / ${total}`;
    const wait = gap - (performance.now() - t0);
    if (wait > 0 && done < total) await sleep(wait);
  }
  if (done) toast(`${active}에 ${done}장 저장했습니다`);
  bursting = false;
  $('burst').innerHTML = '연속 촬영 시작 <kbd>B</kbd>';
  $('burst').classList.add('primary');
  $('burst').classList.remove('danger');
  $('burstState').textContent = '';
  setTimeout(() => { if (!bursting) $('burstBar').style.width = '0'; }, 1200);
}
$('burst').onclick = burst;

// ---------- ZIP download (stored, no compression: JPEG is already compressed) ----------
const CRC = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();
function crc32(buf) {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < buf.length; i++) c = CRC[(c ^ buf[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}
async function makeZip(files) {
  const enc = new TextEncoder();
  const parts = [], central = [];
  let offset = 0;
  for (const f of files) {
    const data = new Uint8Array(await f.blob.arrayBuffer());
    const name = enc.encode(f.name);
    const d = new Date(f.ts);
    const time = (d.getHours() << 11) | (d.getMinutes() << 5) | (d.getSeconds() >> 1);
    const date = ((d.getFullYear() - 1980) << 9) | ((d.getMonth() + 1) << 5) | d.getDate();
    const crc = crc32(data);

    const lh = new DataView(new ArrayBuffer(30));
    lh.setUint32(0, 0x04034b50, true); lh.setUint16(4, 20, true); lh.setUint16(6, 0x0800, true);  // UTF-8 names
    lh.setUint16(8, 0, true); lh.setUint16(10, time, true); lh.setUint16(12, date, true);
    lh.setUint32(14, crc, true); lh.setUint32(18, data.length, true); lh.setUint32(22, data.length, true);
    lh.setUint16(26, name.length, true); lh.setUint16(28, 0, true);
    parts.push(lh.buffer, name, data);

    const ch = new DataView(new ArrayBuffer(46));
    ch.setUint32(0, 0x02014b50, true); ch.setUint16(4, 20, true); ch.setUint16(6, 20, true); ch.setUint16(8, 0x0800, true);
    ch.setUint16(10, 0, true); ch.setUint16(12, time, true); ch.setUint16(14, date, true);
    ch.setUint32(16, crc, true); ch.setUint32(20, data.length, true); ch.setUint32(24, data.length, true);
    ch.setUint16(28, name.length, true); ch.setUint32(42, offset, true);
    central.push(ch.buffer, name);
    offset += 30 + name.length + data.length;
  }
  const cdSize = central.reduce((n, p) => n + p.byteLength, 0);
  const end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true); end.setUint16(8, files.length, true); end.setUint16(10, files.length, true);
  end.setUint32(12, cdSize, true); end.setUint32(16, offset, true);
  return new Blob([...parts, ...central, end.buffer], { type: 'application/zip' });
}
async function downloadZip(label) {
  const list = label ? images.filter(i => i.label === label) : images;
  if (!list.length) return;
  toast('ZIP 만드는 중...');
  const seq = {};
  const files = list.map(i => {
    seq[i.label] = (seq[i.label] || 0) + 1;
    const file = `${i.label}.${String(seq[i.label]).padStart(4, '0')}.jpg`;
    return { name: label ? file : `${i.label}/${file}`, blob: i.blob, ts: i.ts };
  });
  const zip = await makeZip(files);
  const stamp = new Date().toISOString().slice(0, 16).replace(/[-:T]/g, '');
  const a = document.createElement('a');
  a.href = URL.createObjectURL(zip);
  a.download = `${label || 'dataset'}_${stamp}.zip`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  toast(`${files.length}장 ZIP 다운로드`);
}
$('zipAll').onclick = () => downloadZip(null);

// ---------- camera stream + settings ----------
const frame = $('frame'), toggle = $('toggle');
let streamImg = null;
function frameOverlay() {
  return '<div class="flash" id="flash"></div><div class="badge" id="badge"></div><div class="count" id="countdown"></div>';
}
function startStream() {
  streamImg = document.createElement('img');
  streamImg.alt = '카메라 영상';
  streamImg.onerror = () => {
    frame.innerHTML = '<div class="msg err">스트림을 열 수 없습니다. 다른 창에서 보고 있거나 카메라가 연결되지 않았습니다.</div>' + frameOverlay();
    streamImg = null;
    toggle.textContent = '다시 연결';
    updateBadge();
  };
  streamImg.src = 'http://' + BOARD + ':81/stream?t=' + Date.now();
  frame.innerHTML = frameOverlay();
  frame.prepend(streamImg);
  toggle.textContent = '일시정지';
  updateBadge();
}
function stopStream() {
  if (streamImg) { streamImg.src = ''; streamImg = null; }
  frame.innerHTML = '<div class="msg">일시정지됨 (촬영은 계속 가능)</div>' + frameOverlay();
  toggle.textContent = '재생';
  updateBadge();
}
toggle.onclick = () => streamImg ? stopStream() : startStream();
function updateBadge() {
  const b = $('badge');
  if (!b) return;
  b.textContent = active ? '● ' + active : '클래스 없음';
}

function renderCamera(s) {
  $('meta').textContent = `${BOARD} · Wi-Fi ${s.rssi} dBm`;
  $('meta').classList.remove('err');
  if (!s.camera) {
    $('camInfo').textContent = '';
    frame.innerHTML = '<div class="msg err">카메라를 찾지 못했습니다. Sense 확장보드가 꽂혀 있는지 확인하세요.</div>';
    return;
  }
  const sensor = { 0x26: 'OV2640', 0x3660: 'OV3660', 0x5640: 'OV5640' }[s.pid] || ('PID 0x' + s.pid.toString(16));
  $('camInfo').textContent = `${sensor} · ${s.framesize}`;
  $('framesize').value = s.framesize;
  for (const k of ['quality', 'brightness', 'contrast']) { $(k).value = s[k]; $(k + 'Val').textContent = s[k]; }
  $('vflip').checked = !!s.vflip;
  $('hmirror').checked = !!s.hmirror;
}
async function loadStatus() {
  try {
    const r = await fetch('/api/status', { cache: 'no-store' });
    renderCamera(await r.json());
  } catch (e) {
    $('meta').textContent = '보드와 연결이 끊겼습니다';
    $('meta').classList.add('err');
  }
}
async function setCamera(v, val) {
  try {
    const r = await fetch(`/api/camera?var=${v}&val=${encodeURIComponent(val)}`, { cache: 'no-store' });
    if (!r.ok) throw new Error(await r.text());
    renderCamera(await r.json());
  } catch (e) {
    toast('설정 실패: ' + e.message, true);
    loadStatus();
  }
}
$('framesize').onchange = e => setCamera('framesize', e.target.value);
for (const k of ['quality', 'brightness', 'contrast']) {
  $(k).oninput = e => { $(k + 'Val').textContent = e.target.value; };
  $(k).onchange = e => setCamera(k, e.target.value);
}
$('vflip').onchange = e => setCamera('vflip', e.target.checked ? 1 : 0);
$('hmirror').onchange = e => setCamera('hmirror', e.target.checked ? 1 : 0);
setInterval(loadStatus, 5000);

async function showStorage() {
  if (!navigator.storage || !navigator.storage.estimate) return;
  const { usage } = await navigator.storage.estimate();
  $('storage').textContent = `브라우저 저장 ${(usage / 1048576).toFixed(1)} MB`;
}

// ---------- keyboard ----------
document.addEventListener('keydown', e => {
  if (e.target.matches('input[type=text], input[type=number], select')) return;
  if (vId !== null) {
    if (e.key === 'Escape') closeViewer();
    else if (e.key === 'ArrowLeft') stepViewer(-1);
    else if (e.key === 'ArrowRight') stepViewer(1);
    else if (e.key === 'Delete' || e.key === 'Backspace') { e.preventDefault(); deleteInViewer(); }
    return;
  }
  if (e.code === 'Space') { e.preventDefault(); if (!e.repeat) $('shot').click(); }
  else if (e.key === 'z' || e.key === 'Z') $('undo').click();
  else if (e.key === 'b' || e.key === 'B') burst();
  else if (/^[1-9]$/.test(e.key) && classes[e.key - 1]) setActive(classes[e.key - 1].name);
});

// ---------- start ----------
function renderAll() {
  renderClasses();
  renderTabs();
  renderGallery();
  updateBadge();
  showStorage();
}
$('addForm').onsubmit = e => {
  e.preventDefault();
  addClass($('newClass').value).then(renderAll);
  $('newClass').value = '';
};

(async () => {
  startStream();
  loadStatus();
  try {
    db = await openDb();
    if (navigator.storage && navigator.storage.persist) navigator.storage.persist();
    classes = (await getAll('classes')).sort((a, b) => a.created - b.created);
    images = (await getAll('images')).sort((a, b) => a.id - b.id);
    // classes that only exist in images (e.g. after a manual DB edit) still show up
    for (const l of new Set(images.map(i => i.label))) {
      if (!classes.some(c => c.name === l)) classes.push({ name: l, created: Date.now() });
    }
    let saved = null;
    try { saved = localStorage.getItem('ei-active'); } catch (e) {}
    active = classes.some(c => c.name === saved) ? saved : (classes[0] ? classes[0].name : null);
  } catch (e) {
    toast('브라우저 저장소를 열 수 없습니다 (시크릿 창?): ' + e.message, true);
  }
  renderAll();
})();
</script>
</body>
</html>
)rawliteral";
