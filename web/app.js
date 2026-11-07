'use strict';
// Page logic. The C++ engine runs in worker.js; every call returns a promise.

const worker = new Worker('worker.js');
let nextId = 0;
const waiting = new Map();
worker.onmessage = ({ data }) => {
  const p = waiting.get(data.id);
  if (!p) return;
  waiting.delete(data.id);
  data.error !== undefined ? p.reject(new Error(data.error)) : p.resolve(data.result);
};
worker.onerror = (e) => fail('The engine failed to load: ' + (e.message || 'unknown error'));
function call(fn, ...args) {
  return new Promise((resolve, reject) => {
    const id = nextId++;
    waiting.set(id, { resolve, reject });
    worker.postMessage({ id, fn, args });
  });
}
// Move text from the engine ends with its score ("8F WEB 24"); the page shows the score separately.
const plain = (text) => text.replace(/ -?\d+$/, '');
const json = async (fn, ...args) => JSON.parse(await call(fn, ...args));

const $ = (id) => document.getElementById(id);
const el = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
};
function fail(msg) {
  const s = $('loading');
  s.textContent = msg;
  s.classList.add('error');
}

// ---------- Tabs ----------
for (const tab of document.querySelectorAll('[role=tab]')) {
  tab.addEventListener('click', () => {
    for (const t of document.querySelectorAll('[role=tab]')) {
      const on = t === tab;
      t.setAttribute('aria-selected', on);
      $(t.getAttribute('aria-controls')).hidden = !on;
    }
    // The Word Hunt clock starts the first time that tab is opened.
    if (tab.id === 'tab-hunt' && !hunt && layout) newHunt();
  });
}

// ---------- Tessera ----------
const N = 15;
let layout = null;   // { premium: "0..4" x225, values: [26] }
let st = null;       // engine state
let rack = [];       // the human's tiles in display order: { t, used, marked }
let pending = [];    // tiles placed this turn: { r, c, t, i } (i = rack index)
let selected = -1;
let swapMode = false;
let busy = false;
const PREMIUM_LABEL = ['', 'DL', 'TL', 'DW', 'TW'];

function say(text, error) {
  const m = $('message');
  m.textContent = text;
  m.classList.toggle('error', !!error);
}

function tileValue(t) {
  if (t === '?' || (t >= 'A' && t <= 'Z')) return 0;
  return layout.values[t.charCodeAt(0) - 97];
}

async function refresh() {
  st = await json('state');
  rack = st.rack.split('').map((t) => ({ t, used: false, marked: false }));
  pending = [];
  selected = -1;
  draw();
}

function draw() {
  // Board
  const board = $('board');
  board.textContent = '';
  const last = new Set();
  for (let i = st.history.length - 1; i >= 0; --i) {
    if (st.history[i].move.kind === 'place') {
      for (const [r, c] of st.history[i].move.tiles) last.add(r * N + c);
      break;
    }
  }
  const pend = new Map(pending.map((p) => [p.r * N + p.c, p]));
  for (let r = 0; r < N; ++r) {
    for (let c = 0; c < N; ++c) {
      const k = r * N + c;
      const sq = el('button', 'sq p' + layout.premium[k]);
      sq.dataset.r = r;
      sq.dataset.c = c;
      const t = st.board[k] !== '.' ? st.board[k] : pend.has(k) ? pend.get(k).t : null;
      if (t) {
        sq.classList.add('filled');
        if (pend.has(k)) sq.classList.add('pending');
        else if (last.has(k)) sq.classList.add('last');
        if (t >= 'A' && t <= 'Z') sq.classList.add('blank');
        sq.textContent = t.toUpperCase();
        const v = tileValue(t);
        if (v) sq.append(el('span', 'pts', v));
        sq.setAttribute('aria-label', `${t.toUpperCase()} at row ${r + 1} column ${c + 1}`);
      } else {
        sq.textContent = PREMIUM_LABEL[layout.premium[k]];
        if (r === 7 && c === 7) {
          sq.textContent = '';
          sq.classList.add('center');
        }
        sq.setAttribute('aria-label', `Empty square row ${r + 1} column ${c + 1}`);
      }
      board.append(sq);
    }
  }

  // Rack
  const rk = $('rack');
  rk.textContent = '';
  rack.forEach((tile, i) => {
    if (tile.used) return;
    const b = el('button', 'tile', tile.t === '?' ? '?' : tile.t.toUpperCase());
    if (i === selected) b.classList.add('selected');
    if (tile.marked) b.classList.add('marked');
    const v = tileValue(tile.t);
    if (v) b.append(el('span', 'pts', v));
    b.dataset.i = i;
    b.setAttribute('aria-label', tile.t === '?' ? 'Blank tile' : `Tile ${tile.t.toUpperCase()}`);
    rk.append(b);
  });

  // Scores and log
  $('score-you').textContent = st.scores[0];
  $('score-ai').textContent = st.scores[1];
  $('bag').textContent = st.bag;
  const log = $('log');
  log.textContent = '';
  for (const h of st.history) {
    const who = h.player === 0 ? 'You' : 'AI';
    const li = el('li', h.player === 1 ? 'ai' : '', `${who}: ${plain(h.move.text)} (+${h.move.score}) → ${h.total}`);
    log.append(li);
  }
  log.scrollTop = log.scrollHeight;

  const myTurn = !st.over && st.toMove === 0 && !busy;
  for (const id of ['play', 'recall', 'shuffle', 'swap', 'pass', 'hint']) $(id).disabled = !myTurn;
  $('swap').setAttribute('aria-pressed', swapMode);
  $('swap').textContent = swapMode ? 'Swap marked' : 'Swap';
  if (st.over) {
    const [a, b] = st.scores;
    const res = a > b ? 'You win!' : a < b ? 'The AI wins.' : "It's a draw.";
    say(`Game over (${st.endReason}). ${res} ${a}–${b}.` + (st.aiRack !== undefined ? ` The AI was left with: ${st.aiRack ? st.aiRack.toUpperCase() : 'nothing'}.` : ''));
  }
}

$('rack').addEventListener('click', (e) => {
  const b = e.target.closest('.tile');
  if (!b) return;
  const i = +b.dataset.i;
  if (swapMode) rack[i].marked = !rack[i].marked;
  else selected = selected === i ? -1 : i;
  draw();
});

$('board').addEventListener('click', async (e) => {
  const sq = e.target.closest('.sq');
  if (!sq || busy || st.over || st.toMove !== 0 || swapMode) return;
  const r = +sq.dataset.r, c = +sq.dataset.c;
  const at = pending.findIndex((p) => p.r === r && p.c === c);
  if (at >= 0) {
    rack[pending[at].i].used = false;
    pending.splice(at, 1);
    draw();
    return;
  }
  if (st.board[r * N + c] !== '.' || selected < 0) return;
  let t = rack[selected].t;
  if (t === '?') {
    t = await pickBlank();
    if (!t) return;
  }
  pending.push({ r, c, t, i: selected });
  rack[selected].used = true;
  selected = rack.findIndex((x) => !x.used);
  draw();
});

function pickBlank() {
  const dlg = $('blank-dialog');
  const box = $('blank-letters');
  if (!box.childElementCount) {
    for (let i = 0; i < 26; ++i) {
      const L = String.fromCharCode(65 + i);
      const b = el('button', '', L);
      b.value = L;
      box.append(b);
    }
  }
  return new Promise((resolve) => {
    dlg.addEventListener('close', () => resolve(/^[A-Z]$/.test(dlg.returnValue) ? dlg.returnValue : null), { once: true });
    dlg.returnValue = '';
    dlg.showModal();
  });
}

function recall() {
  for (const t of rack) t.used = t.marked = false;
  pending = [];
  draw();
}

$('recall').onclick = recall;
$('shuffle').onclick = () => {
  recall();
  for (let i = rack.length - 1; i > 0; --i) {
    const j = Math.floor(Math.random() * (i + 1));
    [rack[i], rack[j]] = [rack[j], rack[i]];
  }
  selected = -1;
  draw();
};

async function human(fn, ...args) {
  busy = true;
  draw();
  let err;
  try {
    err = await call(fn, ...args);
  } catch (e) {
    err = e.message;
  }
  busy = false;
  if (err) {
    say(err.charAt(0).toUpperCase() + err.slice(1) + '.', true);
    draw();
    return;
  }
  swapMode = false;
  say('');
  await refresh();
  await aiMove();
}

$('play').onclick = () => {
  if (!pending.length) return say('Place some tiles first: click a tile in your rack, then a square.', true);
  human('playTiles', pending.map((p) => `${p.r},${p.c},${p.t}`).join(';'));
};
$('pass').onclick = () => human('pass');
$('swap').onclick = () => {
  if (!swapMode) {
    recall();
    swapMode = true;
    say('Click the tiles you want to swap, then press "Swap marked". Press it with nothing marked to cancel.');
    return draw();
  }
  const give = rack.filter((t) => t.marked).map((t) => t.t).join('');
  if (!give) {
    swapMode = false;
    say('');
    return recall();
  }
  human('exchange', give);
};
$('hint').onclick = async () => {
  busy = true;
  draw();
  say('Looking for a good move…');
  try {
    const res = await json('aiTurn', 'hard', 300, false);
    say(`Hint: ${plain(res.move.text)}` + (res.move.kind === 'place' ? ` for ${res.move.score} points.` : '.'));
  } catch (e) {
    say(e.message, true);
  }
  busy = false;
  draw();
};

async function aiMove() {
  if (st.over || st.toMove !== 1) return;
  busy = true;
  draw();
  const level = $('level').value;
  const ms = +$('think').value;
  say(level === 'expert' ? 'The AI is looking ahead…' : 'The AI is thinking…');
  try {
    const res = await json('aiTurn', level, ms, true);
    showThinking(res);
    busy = false;
    await refresh();
    if (!st.over) say(`AI played ${plain(res.move.text)}` + (res.move.kind === 'place' ? ` for ${res.move.score}.` : '.'));
  } catch (e) {
    busy = false;
    say('AI error: ' + e.message, true);
    draw();
  }
}

function showThinking({ report }) {
  const how = {
    random: 'picked a random reasonable move',
    greedy: 'took the highest-scoring move',
    equity: 'ranked moves by score plus the value of the tiles it keeps',
    simulation: `played ${report.iterations} sample futures (2 turns deep) for its best candidates`,
    endgame: `searched every reply to depth ${report.depth} (bag is empty, so it can see your tiles)`,
  }[report.method] || report.method;
  $('think-summary').textContent = `Out of ${report.moves} legal moves it ${how}, in ${report.ms} ms.`;
  const top = $('think-top');
  top.textContent = '';
  for (const c of report.top.slice(0, 6)) {
    const extra = c.n ? ` · avg outcome ${c.sim.toFixed(1)} over ${c.n} futures` : '';
    top.append(el('li', '', `${plain(c.text)} · equity ${c.equity.toFixed(1)}${extra}`));
  }
}

async function newGame() {
  swapMode = false;
  busy = false;
  await call('newGame', Math.floor(Math.random() * 1e9));
  $('think-summary').textContent = '';
  $('think-top').textContent = '';
  say('Your turn. Click a tile, then a square.');
  await refresh();
  await aiMove();
}
$('new-game').onclick = newGame;

// ---------- Word Hunt ----------
const HUNT = { easy: { every: 7000, maxLen: 5 }, medium: { every: 4500, maxLen: 7 }, hard: { every: 2500, maxLen: 99 } };
const ROUND_MS = 3 * 60 * 1000;
let hunt = null;

function huntSay(text, error) {
  const m = $('hunt-message');
  m.textContent = text;
  m.classList.toggle('error', !!error);
}
const cellsOf = (w) => Array.from({ length: w.w.length }, (_, i) => (w.r + i * w.dr) * hunt.cols + (w.c + i * w.dc));
const points = (w) => w.length - 3;

async function newHunt() {
  stopHunt();
  const g = await json('huntGrid', 12, 12, Math.floor(Math.random() * 1e9), 4);
  // One entry per distinct word (a word can appear more than once in the grid).
  const byWord = new Map();
  for (const w of g.words) if (!byWord.has(w.w)) byWord.set(w.w, []);
  for (const w of g.words) byWord.get(w.w).push(w);
  hunt = {
    rows: g.rows, cols: g.cols, cells: g.cells, byWord,
    owner: new Map(),  // word -> 'you' | 'ai'
    marks: new Map(),  // cell -> Set of owners
    score: { you: 0, ai: 0 },
    ends: Date.now() + ROUND_MS,
    timers: [],
    sel: null,
  };
  const grid = $('hunt-grid');
  grid.textContent = '';
  grid.style.gridTemplateColumns = `repeat(${g.cols}, 1fr)`;
  for (let i = 0; i < g.cells.length; ++i) {
    const c = el('div', 'cell', g.cells[i]);
    c.dataset.i = i;
    grid.append(c);
  }
  huntSay(`${byWord.size} words are hidden. Drag across one, or tap its first and last letter.`);
  $('hunt-you-list').textContent = '';
  $('hunt-ai-list').textContent = '';
  drawHunt();
  hunt.timers.push(setInterval(tickClock, 250));
  scheduleAi();
}

function stopHunt() {
  if (hunt) for (const t of hunt.timers) clearInterval(t), clearTimeout(t);
}

function tickClock() {
  const left = Math.max(0, hunt.ends - Date.now());
  const s = Math.ceil(left / 1000);
  $('hunt-time').textContent = `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
  if (left === 0) endHunt();
}

function scheduleAi() {
  const cfg = HUNT[$('hunt-level').value];
  const wait = cfg.every * (0.6 + Math.random() * 0.8);
  hunt.timers.push(setTimeout(() => {
    if (!hunt || hunt.over) return;
    const open = [...hunt.byWord.keys()].filter((w) => !hunt.owner.has(w) && w.length <= cfg.maxLen);
    if (open.length) {
      // Hard prefers long words; the others pick at random.
      open.sort((a, b) => b.length - a.length);
      const w = $('hunt-level').value === 'hard' ? open[Math.floor(Math.random() * Math.min(3, open.length))]
        : open[Math.floor(Math.random() * open.length)];
      claim(w, 'ai', hunt.byWord.get(w)[0]);
    }
    scheduleAi();
  }, wait));
}

function claim(word, who, place) {
  hunt.owner.set(word, who);
  hunt.score[who] += points(word);
  for (const k of cellsOf(place)) {
    if (!hunt.marks.has(k)) hunt.marks.set(k, new Set());
    hunt.marks.get(k).add(who);
  }
  const li = el('li', '', `${word.toUpperCase()} (+${points(word)})`);
  $(who === 'you' ? 'hunt-you-list' : 'hunt-ai-list').prepend(li);
  if (who === 'ai') huntSay(`The AI found ${word.toUpperCase()}.`);
  drawHunt();
  if (hunt.owner.size === hunt.byWord.size) endHunt();
}

function drawHunt() {
  const sel = new Set(hunt.sel ? hunt.sel.cells : []);
  for (const c of $('hunt-grid').children) {
    const k = +c.dataset.i;
    const m = hunt.marks.get(k);
    c.classList.toggle('you', !!m && m.has('you'));
    c.classList.toggle('ai', !!m && m.has('ai'));
    c.classList.toggle('sel', sel.has(k));
  }
  $('hunt-you').textContent = hunt.score.you;
  $('hunt-ai').textContent = hunt.score.ai;
  $('hunt-left').textContent = `${hunt.byWord.size - hunt.owner.size} of ${hunt.byWord.size} words left.`;
}

function endHunt() {
  if (hunt.over) return;
  hunt.over = true;
  stopHunt();
  $('hunt-time').textContent = '0:00';
  const missed = [...hunt.byWord.keys()].filter((w) => !hunt.owner.has(w)).sort((a, b) => b.length - a.length);
  const { you, ai } = hunt.score;
  const res = you > ai ? 'You win!' : you < ai ? 'The AI wins.' : "It's a draw.";
  huntSay(`Time! ${res} ${you}–${ai}.` + (missed.length ? ` Nobody found: ${missed.slice(0, 12).join(', ').toUpperCase()}${missed.length > 12 ? '…' : ''}` : ''));
}

// Cells on the straight line from a to b (or null if they are not in a line).
function line(a, b) {
  const cols = hunt.cols;
  const ar = Math.floor(a / cols), ac = a % cols, br = Math.floor(b / cols), bc = b % cols;
  const dr = Math.sign(br - ar), dc = Math.sign(bc - ac);
  const n = Math.max(Math.abs(br - ar), Math.abs(bc - ac));
  if (dr && dc && Math.abs(br - ar) !== Math.abs(bc - ac)) return null;
  return { r: ar, c: ac, dr, dc, cells: Array.from({ length: n + 1 }, (_, i) => (ar + i * dr) * cols + ac + i * dc) };
}

function trySelection(s) {
  const word = s.cells.map((k) => hunt.cells[k]).join('');
  const places = hunt.byWord.get(word);
  const place = places && places.find((p) => p.r === s.r && p.c === s.c && p.dr === s.dr && p.dc === s.dc);
  if (!place) return huntSay(word.length >= 4 ? `${word.toUpperCase()} isn't one of the hidden words.` : 'Words have at least 4 letters.', true);
  if (hunt.owner.has(word)) return huntSay(`${word.toUpperCase()} was already found by ${hunt.owner.get(word) === 'you' ? 'you' : 'the AI'}.`, true);
  claim(word, 'you', place);
  huntSay(`Found ${word.toUpperCase()} (+${points(word)}).`);
}

let dragStart = null, tapStart = null, moved = false;
const cellAt = (e) => {
  const t = document.elementFromPoint(e.clientX, e.clientY);
  return t && t.classList.contains('cell') ? +t.dataset.i : null;
};
$('hunt-grid').addEventListener('pointerdown', (e) => {
  if (!hunt || hunt.over) return;
  const k = cellAt(e);
  if (k === null) return;
  dragStart = k;
  moved = false;
  hunt.sel = line(k, k);
  drawHunt();
});
window.addEventListener('pointermove', (e) => {
  if (dragStart === null) return;
  const k = cellAt(e);
  if (k === null) return;
  if (k !== dragStart) moved = true;
  const l = line(dragStart, k);
  if (l) {
    hunt.sel = l;
    drawHunt();
  }
});
window.addEventListener('pointerup', () => {
  if (dragStart === null) return;
  const start = dragStart;
  dragStart = null;
  if (moved) {
    trySelection(hunt.sel);
    tapStart = null;
  } else if (tapStart === null) {
    tapStart = start;   // first tap: wait for the last letter
    return;
  } else {
    const l = line(tapStart, start);
    tapStart = null;
    if (l) trySelection(l);
    else huntSay('Pick letters in a straight line.', true);
  }
  hunt.sel = null;
  drawHunt();
});
$('hunt-new').onclick = newHunt;

// ---------- Start ----------
(async () => {
  try {
    const words = await call('ready');
    layout = await json('layout');
    $('loading').textContent = `${words.toLocaleString()} words loaded.`;
    await newGame();
    if (!$('hunt').hidden) await newHunt();
  } catch (e) {
    fail('Could not start: ' + e.message);
  }
})();
