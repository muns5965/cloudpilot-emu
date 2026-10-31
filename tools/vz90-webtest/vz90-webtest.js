#!/usr/bin/env node
// VZ90 web emulator test runner (local, Playwright + headless Chromium).
//
//   node vz90-webtest.js setup  --build <build-pwa dir> --rom <original VZ90 ROM> --dal <bundle> --work <dir>
//   node vz90-webtest.js rounds --build <build-pwa dir> --work <dir> [--rounds N] [--label name]
//   node vz90-webtest.js soak   --build <build-pwa dir> --work <dir> [--loops N] [--label name]   (one browser kept open)
//
// setup: fresh browser profile -> register ROM + DAL bundle -> boot (boot time) -> Welcome steps (each tap must
//        change the screen) -> launcher -> session saved -> profile copied to <work>/baseline-profile.
// rounds: each round copies the baseline profile and runs T0 (restore = baseline screen), T2 (Prefs/General and
//        launching apps from the launcher, every step must answer), T4 (leave and relaunch = same screen), and
//        records T5/T6 data (renderer memory, diag heap and instructions per second).
//
// A step fails on: DAL stop or fatal-error output (g2d:/g2e:/HALErrDisplay), invalid instruction, emscripten
// abort, page error, no screen change where one is expected, or no diag summary for 30 s (emulator stalled).
// MMIO/data aborts reported by the diag build are observations (listed per site), not failures by themselves.
// On a failure: step name, last console lines, diag lines and a screenshot are written to <work>/<label>/.

const fs = require('fs');
const path = require('path');
const http = require('http');
const { execSync } = require('child_process');
const { chromium } = require('playwright');
const sharp = require('sharp');

const args = require('util').parseArgs({
    allowPositionals: true,
    options: {
        build: { type: 'string' }, rom: { type: 'string' }, dal: { type: 'string' }, work: { type: 'string' },
        rounds: { type: 'string', default: '1' }, label: { type: 'string', default: 'run' },
        port: { type: 'string', default: '8790' }, loops: { type: 'string', default: '4' },   // fixed: browser storage (the saved session) belongs to the origin
    },
});
const MODE = args.positionals[0];
const O = args.values;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const now = () => Date.now();

// ---------------------------------------------------------------- static server for the built PWA
function serve(dir) {
    const types = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css',
        '.wasm': 'application/wasm', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png',
        '.md': 'text/markdown', '.ico': 'image/x-icon', '.woff2': 'font/woff2', '.ttf': 'font/ttf' };
    const server = http.createServer((req, res) => {
        let p = decodeURIComponent(req.url.split('?')[0]);
        if (p.endsWith('/')) p += 'index.html';
        let f = path.join(dir, p);
        if (!f.startsWith(dir) || !fs.existsSync(f)) f = path.join(dir, 'index.html');
        res.writeHead(200, { 'content-type': types[path.extname(f)] || 'application/octet-stream' });
        fs.createReadStream(f).pipe(res);
    });
    return new Promise((resolve) => server.listen(+O.port, '127.0.0.1', () => resolve(server)));
}

// ---------------------------------------------------------------- console capture and classification
const FAIL_PATTERNS = [
    [/g2d: CALL table/, 'DAL stop (call without implementation)'],
    [/g2e: stopped/, 'DAL stopped'],
    [/HALErrDisplay/, 'OS fatal error (HALErrDisplay)'],
    [/Invalid instr/, 'invalid instruction'],
    [/Aborted\(|RuntimeError|unreachable/, 'emscripten abort / wasm trap'],
    [/g1: EXCEPTION vector/, 'DAL exception handler entered'],
    [/g2: DATA ABORT/, 'DAL data abort report'],
    // decided on the frame line that follows each "[diag] pace-fault", so the 68k signature is complete
    [/memory fault in PACE|\[diag\] pace-frames/, 'memory fault in PACE (68k code)'],
    // the PWA's own storage failing (its "IndexedDB access failed" dialog); not part of any known fault
    [/IDBDatabase|IndexedDB access failed|QuotaExceededError/, 'browser storage (IndexedDB) error'],
    // "[idb] forced close" (the browser closing the connection) is a diagnostic event, not a failure by itself; the
    // failure is what the user meets: the snapshot store failing (InvalidStateError above) or the PWA's fatal dialog
];

// Known faults, reproduced the same way with the reference DAL: matched by step and failure text, recorded under
// "known" instead of counting as new failures. A known fault that no longer appears is reported too.
const PACE_FRAME_TIMEOUT = 3000;
const KNOWN = [
    // identified by its 68k signature, independent of the ROM layout: opcode CLR.B (A2) writing address 0, reported
    // 68k pc minus first return address 0x1ac8, first minus second return address 0x2402 (CLIE_Org_Address code 1;
    // the faulting CLR.B is at 0x4d6a, the reported pc is the next instruction 0x4d6c because the pc has advanced
    // past the fetch: 0x4d6c - 0x32a4 = 0x1ac8; measured the same on the reference and the rebuilt ROM)
    { id: 'memo-button-pace-null-write', step: /^K memo button/, what: /memory fault in PACE|no response|not back on the launcher/,
      signature: { op: '4212', addr: '00000000', d1: 0x1ac8, d2: 0x2402 },
      note: 'Memo hard button: CLIE_Org_Address writes to address 0 (PACE memory fault); same with the reference DAL (FAULT: DABT screen there, black screen here); condition of the path undetermined. Fixed on VZ90 images by RAM at physical 0 (emulator 13a12502); the round now runs the button as T5, so this entry only serves the self-test' },
];

class Session {
    constructor(page, log) {
        this.page = page; this.log = log; this.lines = []; this.fails = []; this.diag = []; this.summaries = [];
        this.step = 'start'; this.lastSummaryAt = 0; this.sites = new Map(); this.mmio = []; this.shared = []; this.known = []; this.pace = []; this.paceSeq = 0;
        const take = (kind, text) => {
            const line = `${((now() - log.t0) / 1000).toFixed(1)} [${this.step}] ${kind} ${text}`;
            this.lines.push(line); if (this.lines.length > 5000) this.lines.shift();
            if (/\[diag\]/.test(text)) this.onDiag(text.replace(/^\d+: /, ''));
            for (const [re, what] of FAIL_PATTERNS) if (re.test(text)) this.fail(what, text);
        };
        this.take = take;
        page.on('console', (m) => take(m.type(), m.text()));
        page.on('pageerror', (e) => { take('pageerror', String(e)); this.fail('page error', String(e)); });
        page.on('crash', () => this.fail('renderer crash', ''));
    }
    onDiag(t) {
        this.diag.push(`${this.step} ${t}`);
        const s = t.match(/summary t=([\d.]+) ips=(\d+) ipsmax=(\d+) heap=(\d+) aborts=(\d+) unique=(\d+) undef=(\d+)/);
        if (s) { this.lastSummaryAt = now(); this.summaries.push({ at: now(), step: this.step, ips: +s[2], ipsMax: +s[3], heapMiB: +s[4], aborts: +s[5], unique: +s[6], undef: +s[7] }); }
        const a = t.match(/abort (\w+) (\w) addr=(\w+) region=([\w-]+) pc=(\w+) at=(\w+)/);
        if (a) this.sites.set(`${a[3]}@${a[5]}`, { t: now(), step: this.step, kind: a[1], rw: a[2], addr: a[3], region: a[4], pc: a[5], at: a[6] });
        const m = t.match(/mmio first (\w) addr=(\w+) size=(\d+) dev=([\w-]+) pc=(\w+) at=(\w+) ok=(\d)/);
        if (m) this.mmio.push({ step: this.step, rw: m[1], addr: m[2], size: +m[3], dev: m[4], pc: m[5], at: m[6], ok: m[7] === '1' });
        // each pace-fault opens a pending event (sequence number); the frame line printed right after it completes
        // the signature. An event left without frames past PACE_FRAME_TIMEOUT is reported as incomplete.
        const pf = t.match(/pace-fault (\w) addr=(\w+) pc=\w+ at=\w+ pc68k=(\w+) op=(\w+)/);
        if (pf) this.pace.push({ seq: ++this.paceSeq, at: now(), step: this.step, rw: pf[1], addr: pf[2], pc68k: parseInt(pf[3], 16), op: pf[4], pending: true });
        const fr = t.match(/pace-frames(?: a6=\w+ ret=(\w+))?(?: a6=\w+ ret=(\w+))?/);
        if (fr) {
            const p = [...this.pace].reverse().find((x) => x.pending);
            if (p) { const r1 = fr[1] ? parseInt(fr[1], 16) : 0, r2 = fr[2] ? parseInt(fr[2], 16) : 0;
                p.d1 = r1 ? p.pc68k - r1 : null; p.d2 = r1 && r2 ? r1 - r2 : null; p.pending = false; p.frameFor = p.seq; }
        }
        const sh = t.match(/shared w addr=(\w+) size=(\d+) value=(\w+) pc=(\w+) at=(\w+)/);
        if (sh) this.shared.push({ step: this.step, addr: sh[1], size: +sh[2], value: sh[3], pc: sh[4], at: sh[5] });
        const u = t.match(/undef instr=(\w+) pc=(\w+) at=(\w+)/);
        if (u) this.sites.set(`undef@${u[2]}`, { t: now(), step: this.step, kind: 'undef', instr: u[1], pc: u[2], at: u[3] });
    }
    // PACE faults whose frame line did not arrive in time: diagnostics incomplete, never a known fault
    checkPending(timeoutMs = PACE_FRAME_TIMEOUT) {
        for (const p of this.pace) if (p.pending && now() - p.at > timeoutMs) {
            p.pending = false; p.incomplete = true;
            this.fail('PACE fault with incomplete diagnostics (no frame line)', `seq ${p.seq} op ${p.op} addr ${p.addr} pc68k ${p.pc68k.toString(16)}`);
        }
    }
    fail(what, detail) {
        const f = { step: this.step, what, detail: String(detail).slice(0, 300), at: now(), diskFreeGiB: this.diskNow ? this.diskNow() : null };
        // a known fault needs its step, its failure text and, when it has one, the 68k signature seen in this run
        const sigOk = (x) => !x.signature || this.pace.some((p) => !p.pending && p.op === x.signature.op && p.addr === x.signature.addr && p.d1 === x.signature.d1 && p.d2 === x.signature.d2);
        const k = KNOWN.find((x) => x.step.test(f.step) && x.what.test(f.what) && sigOk(x));
        if (k) { this.known.push({ ...f, known: k.id }); return; }
        this.fails.push(f);
    }
}

// ---------------------------------------------------------------- browser, app navigation, device input
// free disk space where the browser profile lives. Chromium refuses storage writes when free space is low; in this
// environment the PWA's snapshot store then fails ("IndexedDB access failed", database connection closing) - an
// environment condition, not an emulator one. Recorded at each browser start; below DISK_MIN a warning is printed.
const DISK_MIN = 3 * 1024 ** 3;
function diskFree(dir) { try { const st = fs.statfsSync(dir); return st.bavail * st.bsize; } catch { return -1; } }
// IndexedDB probe, injected before the page: connection open/success/failure, the browser's forced close (the PWA
// does not listen to "close"), versionchange, transaction aborts and errors with their DOMException name, an
// explicit close() by the page with its caller, and navigator.storage.estimate() every 30 s. Lines start with
// "[idb]"; times are seconds since page start.
const IDB_PROBE = () => {
    const T = () => (performance.now() / 1000).toFixed(1);
    const log = (m) => console.log(`[idb] t=${T()} ${m}`);
    const open = indexedDB.open.bind(indexedDB);
    let n = 0;
    indexedDB.open = function (...a) {
        const id = ++n, r = open(...a);
        log(`open #${id} ${a[0]} v${a[1]}`);
        r.addEventListener('error', () => log(`open #${id} failed ${r.error && r.error.name}: ${r.error && r.error.message}`));
        r.addEventListener('blocked', () => log(`open #${id} blocked`));
        r.addEventListener('success', () => {
            const db = r.result; log(`open #${id} ok`);
            db.addEventListener('close', () => log(`forced close #${id} (connection closed by the browser)`));
            db.addEventListener('versionchange', (e) => log(`versionchange #${id} ${e.oldVersion}->${e.newVersion}`));
            db.addEventListener('abort', (e) => { const x = e.target && e.target.error; log(`tx abort #${id} ${x ? x.name + ': ' + x.message : '(no error, aborted by page)'}`); }, true);
            db.addEventListener('error', (e) => { const x = e.target && e.target.error; log(`request error #${id} ${x ? x.name + ': ' + x.message : ''}`); }, true);
            db.__probeId = id;
        });
        return r;
    };
    const close = IDBDatabase.prototype.close;
    IDBDatabase.prototype.close = function () { log(`close() by page #${this.__probeId} at ${(new Error().stack || '').split('\n')[2] || ''}`.trim()); return close.call(this); };
    setInterval(() => { if (navigator.storage && navigator.storage.estimate) navigator.storage.estimate().then((e) => log(`estimate usage=${e.usage} quota=${e.quota}`), (x) => log(`estimate failed ${x}`)); }, 30000);
};
async function openBrowser(profile, url, log) {
    const free = diskFree(path.dirname(profile)); (log.diskFreeGiB ||= []).push(+(free / 1024 ** 3).toFixed(2));
    if (free >= 0 && free < DISK_MIN) console.log(`  WARNING disk: ${(free / 1024 ** 3).toFixed(2)} GiB free (< ${DISK_MIN / 1024 ** 3} GiB): browser storage writes may be refused, results not valid for storage-related steps`);
    const ctx = await chromium.launchPersistentContext(profile, { viewport: { width: 1000, height: 800 } });
    await ctx.addInitScript(IDB_PROBE);
    const page = ctx.pages()[0] || (await ctx.newPage());
    const s = new Session(page, log);
    // free disk every 10 s in the console record, and at each failure
    const gib = () => +(diskFree(path.dirname(profile)) / 1024 ** 3).toFixed(2);
    s.diskNow = gib; (log.diskSamples ||= []);
    const iv = setInterval(() => { const g = gib(); log.diskSamples.push([+((now() - log.t0) / 1000).toFixed(0), g]); s.take('disk', `[disk] free=${g}GiB`); }, 10000);
    iv.unref(); ctx.on('close', () => clearInterval(iv));
    await page.goto(url, { waitUntil: 'networkidle' });
    await sleep(2500);
    return { ctx, page, s };
}

async function tabSessions(page) { await page.locator('ion-tab-button', { hasText: 'Sessions' }).click(); await sleep(1200); }

async function launchSession(page, name) {
    await tabSessions(page);
    await page.locator('ion-item', { hasText: name }).first().click();
    await page.waitForSelector('canvas', { state: 'visible', timeout: 20000 });
    await sleep(1500);
}

// device (x, y) in the 320x480 PV frame -> page coordinates (same transform the app inverts for input)
async function geometry(page) {
    return page.$eval('app-emulation canvas, canvas', (c) => {
        const bb = c.getBoundingClientRect(); const w = c.width, h = c.height;
        let cw, ch, cx, cy;
        if (bb.width / bb.height > w / h) { ch = bb.height; cw = (w / h) * bb.height; cy = bb.top; cx = bb.left + (bb.width - cw) / 2; }
        else { cw = bb.width; ch = (h / w) * bb.width; cx = bb.left; cy = bb.top + (bb.height - ch) / 2; }
        return { cx, cy, unit: cw / 322 };   // frame width = 1 + 320 + 1 device units
    });
}
const toPage = (g, x, y) => ({ x: g.cx + (x + 1) * g.unit, y: g.cy + (y + 1) * g.unit });

async function tap(page, x, y) {
    const g = await geometry(page); const p = toPage(g, x, y);
    await page.mouse.move(p.x, p.y); await page.mouse.down(); await sleep(150); await page.mouse.up();
}

// screen without the status bar (clock): device rows 0..454
async function screen(page) {
    const g = await geometry(page); const a = toPage(g, 0, 0), b = toPage(g, 320, 455);
    const png = await page.screenshot({ clip: { x: a.x, y: a.y, width: b.x - a.x, height: b.y - a.y } });
    const { data, info } = await sharp(png).resize(160, 228, { kernel: 'nearest' }).raw().toBuffer({ resolveWithObject: true });
    return { png, data, info };
}
// calibration screen: black background with one bright cross target; returns its centre in device units
function calibrationTarget(scr) {
    if (!scr || hasHeaderTab(scr)) return null;                                  // header tab -> text screen
    // which target is shown is read from the screen; the tap goes to the point that calibrated correctly in the
    // native run (the stylus picture next to the cross would bias a centroid)
    const boxes = [['top-left', 0, 0, 40, 40, 20, 20], ['bottom-right', 280, 280, 320, 320, 300, 300], ['centre', 140, 100, 180, 180, 160, 120]];
    for (const [where, x0, y0, x1, y1, tx, ty] of boxes) {
        let sx = 0, sy = 0, k = 0;
        for (let y = y0 >> 1; y < y1 >> 1; y++) for (let x = x0 >> 1; x < x1 >> 1; x++) {
            const i = (y * 160 + x) * 3; if (scr.data[i] + scr.data[i + 1] + scr.data[i + 2] > 450) { sx += x; sy += y; k++; } }
        if (k >= 12) return { where, x: tx, y: ty };
    }
    return null;
}
// Welcome text screens have a light header tab at the top left
function hasHeaderTab(scr) { return brightFraction(scr, 36, 3, 56, 11, 360) > 0.5; }   // right part of the tab, clear of a corner target
function brightFraction(scr, x0, y0, x1, y1, min) {
    let k = 0, n = 0;
    for (let y = y0 >> 1; y < y1 >> 1; y++) for (let x = x0 >> 1; x < x1 >> 1; x++) { const i = (y * 160 + x) * 3; n++; if (scr.data[i] + scr.data[i + 1] + scr.data[i + 2] > min) k++; }
    return n ? k / n : 0;
}
// mean brightness of a device rectangle in a screen() capture (160x228 = device / 2)
function regionMean(scr, x0, y0, x1, y1) {
    let sum = 0, n = 0;
    for (let y = y0 >> 1; y < y1 >> 1; y++) for (let x = x0 >> 1; x < x1 >> 1; x++) { const i = (y * 160 + x) * 3; sum += scr.data[i] + scr.data[i + 1] + scr.data[i + 2]; n += 3; }
    return n ? sum / n : 0;
}
// fraction of changed pixels in device rows fromY..toY (e.g. without the launcher title bar and its clock, or
// without the input area at the bottom, which the user can open and close)
function diff(a, b, fromY = 0, toY = 455) {
    if (!a || !b || a.data.length !== b.data.length) return 1;
    const end = Math.min(a.data.length, (toY >> 1) * 160 * 3);
    let n = 0; for (let i = (fromY >> 1) * 160 * 3; i < end; i += 3) if (Math.abs(a.data[i] - b.data[i]) + Math.abs(a.data[i + 1] - b.data[i + 1]) + Math.abs(a.data[i + 2] - b.data[i + 2]) > 24) n++;
    return n / (end / 3 - (fromY >> 1) * 160);
}
const LAUNCHER_ROWS = [18, 290];            // icon grid of the launcher

// one test step: action, wait, screen; expect = 'change' | 'same' | null
// step status: PASS, KNOWN_FAILURE (a KNOWN fault reproduced; not counted as passed), NEW_FAILURE,
// UNEXPECTED_PASS (a step expected to show a known fault did not), NOT_RUN (recorded by the caller)
async function step(ctx, name, action, { wait = 4000, expect = null, ref = null, tolerance = 0.002, rows = [0, 455], expectKnown = false } = {}) {
    const { page, s, log } = ctx;
    s.step = name;
    const before = ctx.last || (await screen(page));
    const failsBefore = s.fails.length, knownBefore = s.known.length;
    const t = now();
    if (action) await action();
    await sleep(wait);
    const after = await screen(page);
    const d = diff(ref || before, after, ...rows);
    let ok = true, note = `diff ${(d * 100).toFixed(2)}%`;
    if (expect === 'change' && d <= tolerance) { ok = false; s.fail('no response (screen unchanged after input)', note); }
    if (expect === 'same' && d > tolerance) { ok = false; s.fail('screen differs from reference', note); }
    // 'launcher': recovered only when the launcher is back (same test as ensureLauncher); a changed screen alone
    // (an error dialog, say) is not a recovery
    if (expect === 'launcher' && d > 0.01) { ok = false; s.fail('not back on the launcher (home did not return, or an application kept the screen)', note); }
    if (s.lastSummaryAt && now() - s.lastSummaryAt > 30000) {
        // no summary for 30 s: suspected stall; record whether the page itself still answers (tab throttling, main
        // thread blocked or log delivery would look the same from here)
        const alive = await Promise.race([page.evaluate(() => performance.now()).then(() => true), sleep(5000).then(() => false)]);
        ok = false; s.fail(`suspected stall: no diag summary for 30 s (page responsive: ${alive ? 'yes' : 'no'})`, '');
        s.lastSummaryAt = now();
    }
    s.checkPending();
    const status = s.fails.length > failsBefore ? 'NEW_FAILURE' : s.known.length > knownBefore ? 'KNOWN_FAILURE' : expectKnown ? 'UNEXPECTED_PASS' : 'PASS';
    ok = status === 'PASS';
    ctx.last = after;
    fs.writeFileSync(path.join(log.dir, `step-${String(log.steps.length + 1).padStart(2, '0')}-${name.replace(/\W+/g, '_')}.png`), after.png);
    log.steps.push({ name, status, ok, ms: now() - t, note });
    if (status === 'NEW_FAILURE' || status === 'KNOWN_FAILURE') await artifacts(ctx, name, after);
    const why = status === 'NEW_FAILURE' ? s.fails.slice(failsBefore).map((f) => f.what) : status === 'KNOWN_FAILURE' ? s.known.slice(knownBefore).map((f) => `${f.known}: ${f.what}`) : [];
    console.log(`  ${status.padEnd(15)} ${name.padEnd(28)} ${note}${why.length ? '  <- ' + why.join('; ') : ''}`);
    return ok;
}

// bring the launcher back to the baseline screen: scroll up, close a dialog (OK at the bottom left), go home
async function ensureLauncher(ctx, ref, name) {
    const { page, s } = ctx; s.step = name;
    const tries = [null, [313, 50], [313, 50], [48, 293], HOME, [313, 50], [313, 50]];
    for (const t of tries) {
        if (t) { await tap(page, ...t); await sleep(1800); }
        ctx.last = await screen(page);
        if (diff(ref, ctx.last, ...LAUNCHER_ROWS) <= 0.01) return true;
    }
    s.fail('not back on the launcher (home did not return, or an application kept the screen)', `diff ${(diff(ref, ctx.last, ...LAUNCHER_ROWS) * 100).toFixed(1)}%`);
    ctx.log.steps.push({ name, status: 'NEW_FAILURE', ok: false, ms: 0, note: '' });
    console.log(`  ${'NEW_FAILURE'.padEnd(15)} ${name.padEnd(28)} <- not back on the launcher`);
    await artifacts(ctx, name, ctx.last);
    return false;
}

async function artifacts(ctx, name, scr) {
    const dir = path.join(ctx.log.dir, 'fail-' + name.replace(/\W+/g, '_'));
    fs.mkdirSync(dir, { recursive: true });
    if (scr) fs.writeFileSync(path.join(dir, 'screen.png'), scr.png);
    await ctx.page.screenshot({ path: path.join(dir, 'page.png') }).catch(() => {});
    fs.writeFileSync(path.join(dir, 'console.txt'), ctx.s.lines.join('\n'));
    fs.writeFileSync(path.join(dir, 'diag.txt'), ctx.s.diag.join('\n'));
    fs.writeFileSync(path.join(dir, 'fails.json'), JSON.stringify(ctx.s.fails, null, 1));
}

function rendererRssMB() {
    try { return Math.round(+execSync("ps -eo rss,args | grep -E 'chrom.*--type=renderer' | grep -v grep | awk '{s+=$1} END {print s+0}'").toString() / 1024); } catch { return 0; }
}

// ---------------------------------------------------------------- scenarios
const HOME = [12, 465];                     // home icon in the status bar (standard launcher once Media Launcher is unassigned)
const MENU = [37, 465];                     // menu icon in the status bar
const WELCOME_AFTER_CALIBRATION = [[160, 200], [52, 293], [125, 302], [125, 302], [125, 302]];
// standard launcher, category "all" (3 columns, rows of 68 device px); Prefs (160,130) is driven separately and the
// Setup application (56,198) is left out (it would restart the Welcome flow)
const PREFS = [160, 130];
// hard buttons below the screen (device y 480..540): Date Book, Address, rocker up/down, To Do, Memo
const BUTTONS = { datebook: [30, 510], address: [90, 510], todo: [230, 510] };
const MEMO_BUTTON = [290, 510];             // Memo hard button (T5, last in a round)
const ROCKER_UP = [160, 494], ROCKER_DOWN = [160, 526];
// Calculator keys (device units) and the display row compared with a golden image
// measured on the Calculator screen: digit columns x 96/162/224, rows y 134/186/239/292
const CALC = { C: [27, 292], 1: [96, 239], 2: [162, 239], 3: [224, 239], 4: [96, 186], 6: [224, 186], plus: [286, 265], eq: [224, 292] };
const CALC_DISPLAY_ROWS = [0, 40];

let BASELINE_ID = '';
// golden images with a manifest: SHA-256 of <file>.raw, the inputs (coordinates, compared rows) it was made with,
// and whether a person confirmed what it shows. calc: Calculator display after 12+34= shows 46. org: the screen the
// Memo hard button opens (CLIE Organizer, Address tab, first launch after setup).
const ORG_ROWS = [0, 290];
const GOLDEN = {
    calc: { file: 'golden-calc', what: 'shows 46', inputs: () => JSON.stringify({ CALC, CALC_DISPLAY_ROWS, keys: ['C', 1, 2, 'plus', 3, 4, 'eq'] }) },
    org: { file: 'golden-org', what: 'CLIE Organizer opened by the Memo button', inputs: () => JSON.stringify({ MEMO_BUTTON, ORG_ROWS }) },
    entrySaved: { file: 'golden-entry-saved', what: 'Address list right after saving: entry てすと present and selected', inputs: () => JSON.stringify({ ORG_ENTRY, ORG_ROWS }) },
    entry: { file: 'golden-entry', what: 'Address list reopened: entry てすと present, not selected', inputs: () => JSON.stringify({ ORG_ENTRY, ORG_ROWS }) },
    edited: { file: 'golden-edited', what: 'Address list right after editing: 2てすと first and selected, てすと gone', inputs: () => JSON.stringify({ ORG_EDIT, ORG_ROWS }) },
    editedReopen: { file: 'golden-edited-reopen', what: 'Address list reopened: 2てすと first, not selected', inputs: () => JSON.stringify({ ORG_EDIT, ORG_ROWS }) },
    deleted: { file: 'golden-deleted', what: 'Address list right after deleting 2てすと: only the hint entry, selected', inputs: () => JSON.stringify({ ORG_EDIT, ORG_ROWS }) },
    deletedReopen: { file: 'golden-deleted-reopen', what: 'Address list reopened after the delete: only the hint entry', inputs: () => JSON.stringify({ ORG_EDIT, ORG_ROWS }) },
};
function writeGoldenManifest(work, verified, g = GOLDEN.calc) {
    const raw = path.join(work, g.file + '.raw');
    fs.writeFileSync(path.join(work, g.file + '.json'), JSON.stringify({ sha256: sha(raw), inputs: g.inputs(), verified,
        verifiedAt: verified ? new Date().toISOString() : null }, null, 1));
}
function goldenState(work, g = GOLDEN.calc) {
    const raw = path.join(work, g.file + '.raw'), man = path.join(work, g.file + '.json');
    if (!fs.existsSync(raw)) return { state: 'missing' };
    if (!fs.existsSync(man)) return { state: 'invalid', why: 'golden image without manifest' };
    const m = JSON.parse(fs.readFileSync(man, 'utf8'));
    if (m.sha256 !== sha(raw)) return { state: 'invalid', why: 'golden image changed after it was recorded' };
    if (m.inputs !== g.inputs()) return { state: 'invalid', why: 'coordinates changed since the golden image was made' };
    return { state: m.verified ? 'verified' : 'unverified' };
}
function verifyGolden() {
    const g = GOLDEN[args.positionals[1] || 'calc'];
    if (!g) { console.log(`verify-golden [${Object.keys(GOLDEN).join('|')}]`); return false; }
    const st = goldenState(O.work, g);
    if (st.state !== 'unverified' && st.state !== 'verified') { console.log(`golden: ${st.state} ${st.why || ''}`); return false; }
    writeGoldenManifest(O.work, true, g); console.log(`golden image ${g.file} marked verified (${g.what})`); return true;
}
// a golden-image step: creates the image (unverified) when missing, compares only a verified one
async function goldenStep(ctx, name, g, cur, rows) {
    const { s } = ctx, work = path.dirname(ctx.log.dir), raw = path.join(work, g.file + '.raw');
    if (goldenState(work, g).state === 'missing') {
        fs.writeFileSync(raw, cur.data); fs.writeFileSync(path.join(work, g.file + '.png'), cur.png);
        writeGoldenManifest(work, false, g);
        console.log(`  note ${name}: golden image created, NOT verified - check ${path.join(work, g.file + '.png')} (${g.what}), then run "verify-golden ${Object.keys(GOLDEN).find((k) => GOLDEN[k] === g)}"`);
    }
    const st = goldenState(work, g);
    if (st.state === 'verified') return step(ctx, name, null, { wait: 0, expect: 'same', ref: { data: fs.readFileSync(raw) }, tolerance: 0.002, rows });
    if (st.state === 'unverified') { ctx.log.steps.push({ name, status: 'NOT_RUN', ok: false, ms: 0, note: 'golden image not verified' }); console.log(`  ${'NOT_RUN'.padEnd(15)} ${name}  golden image not verified`); return; }
    ctx.log.steps.push({ name, status: 'NEW_FAILURE', ok: false, ms: 0, note: st.why }); console.log(`  ${'NEW_FAILURE'.padEnd(15)} ${name}  ${st.why}`); s.fail('golden image invalid', st.why);
}
// T6: CLIE Organizer data: a new Address entry typed on the on-screen kana keyboard, saved, found again after the
// application is reopened and after the session is saved and relaunched. Coordinates measured on screen (explore):
const ORG_ENTRY = {
    addressTab: [311, 75],          // right-hand tab "アドレス" (the Memo button opens the last used tab)
    newShown: [51, 304],            // "new" icon of the Address list while the input area is shown
    showInputArea: [304, 465],      // status bar arrow
    keyboard: [296, 342],           // input area "キーボード" -> kana keyboard
    kana: [[154, 226], [179, 199], [154, 253]],   // て す と
    keyboardDone: [45, 292],        // keyboard "終了"
    formDone: [266, 300],           // edit form "終了" (input area shown)
};
// the Sony input area (dark panel with the IME buttons) occupies device rows ~330..455 when shown
function inputAreaShown(scr) {
    let sum = 0, k = 0;
    for (let y = 175; y < 220; y++) for (let x = 50; x < 125; x++) { const i = (y * 160 + x) * 3; sum += scr.data[i] + scr.data[i + 1] + scr.data[i + 2]; k++; }
    return sum / k / 3 < 80;
}
async function organizerEntry(ctx, ref, base) {
    const { page } = ctx, E = ORG_ENTRY;
    await step(ctx, 'T6 memo button', () => tap(page, ...MEMO_BUTTON), { wait: 7000, expect: 'change', ref, rows: LAUNCHER_ROWS, tolerance: 0.3 });
    await step(ctx, 'T6 Address tab', () => tap(page, ...E.addressTab), { wait: 3000 });
    // the input area keeps its last state (shown after T5 in a round, hidden on a fresh boot): show it first
    if (!inputAreaShown(await screen(page))) await step(ctx, 'T6 show input area', () => tap(page, ...E.showInputArea), { wait: 2500, expect: 'change' });
    await step(ctx, 'T6 new entry form', () => tap(page, ...E.newShown), { wait: 3000, expect: 'change' });
    await step(ctx, 'T6 kana keyboard', () => tap(page, ...E.keyboard), { wait: 2500, expect: 'change' });
    await step(ctx, 'T6 type てすと', async () => { for (const k of E.kana) { await tap(page, ...k); await sleep(700); } }, { wait: 1200, expect: 'change' });
    await step(ctx, 'T6 keyboard done', () => tap(page, ...E.keyboardDone), { wait: 2500, expect: 'change' });
    await step(ctx, 'T6 save entry', () => tap(page, ...E.formDone), { wait: 3000, expect: 'change' });
    await goldenStep(ctx, 'T6 entry in list (golden)', GOLDEN.entrySaved, await screen(page), ORG_ROWS);
    // reopen: home, Memo button, Address tab
    if (!(await backToLauncher(ctx, ref, 'T6 home'))) return;
    await step(ctx, 'T6 reopen', () => tap(page, ...MEMO_BUTTON), { wait: 7000, expect: 'change', ref, rows: LAUNCHER_ROWS, tolerance: 0.3 });
    await step(ctx, 'T6 reopen Address tab', () => tap(page, ...E.addressTab), { wait: 3000 });
    await goldenStep(ctx, 'T6 entry after reopen (golden)', GOLDEN.entry, await screen(page), ORG_ROWS);
    // session saved and relaunched: the entry is still there
    ctx.s.step = 'T6 save session'; await tabSessions(page); await sleep(5000);
    await launchSession(page, base.name); await sleep(6000);
    await step(ctx, 'T6 relaunch Address tab', () => tap(page, ...E.addressTab), { wait: 3000 });
    await goldenStep(ctx, 'T6 entry after session relaunch (golden)', GOLDEN.entry, await screen(page), ORG_ROWS);
    if (await backToLauncher(ctx, ref, 'T6 end home')) await organizerEdit(ctx, ref, base);
}

// T7: the T6 entry edited (a "2" typed in front: 2てすと) and then deleted; each state checked right away, after
// reopening and after a session relaunch. Every action is its own step, so a failure shows whether input, saving or
// the list update failed. Coordinates measured with explore (input area shown, as T6 leaves it).
const ORG_EDIT = {
    rowSecond: [182, 132],          // second list row (てすと below the hint entry)
    rowFirst: [182, 71],            // first list row (2てすと sorts before the hint entry)
    edit: [81, 304],                // view screen: edit icon
    keyboard: [296, 342], numTab: [231, 292], digit2: [163, 142], keyboardDone: [45, 292],
    formDone: [266, 300], formDelete: [173, 300], confirmOk: [51, 292],
};
async function reopenAddress(ctx, ref, base, label, g, relaunch) {
    const { page } = ctx, E = ORG_ENTRY;
    if (relaunch) {
        ctx.s.step = `${label} save session`; await tabSessions(page); await sleep(5000);
        await launchSession(page, base.name); await sleep(6000);
    } else {
        if (!(await backToLauncher(ctx, ref, `${label} home`))) return false;
        await step(ctx, `${label} reopen`, () => tap(page, ...MEMO_BUTTON), { wait: 7000, expect: 'change', ref, rows: LAUNCHER_ROWS, tolerance: 0.3 });
    }
    await step(ctx, `${label} Address tab`, () => tap(page, ...E.addressTab), { wait: 3000 });
    await goldenStep(ctx, `${label} list (golden)`, g, await screen(page), ORG_ROWS);
    return true;
}
async function organizerEdit(ctx, ref, base) {
    const { page } = ctx, E = ORG_EDIT;
    await step(ctx, 'T7 memo button', () => tap(page, ...MEMO_BUTTON), { wait: 7000, expect: 'change', ref, rows: LAUNCHER_ROWS, tolerance: 0.3 });
    await step(ctx, 'T7 Address tab', () => tap(page, ...ORG_ENTRY.addressTab), { wait: 3000 });
    await step(ctx, 'T7 open てすと', () => tap(page, ...E.rowSecond), { wait: 3000, expect: 'change' });
    await step(ctx, 'T7 edit', () => tap(page, ...E.edit), { wait: 3000, expect: 'change' });
    await step(ctx, 'T7 keyboard', () => tap(page, ...E.keyboard), { wait: 2500, expect: 'change' });
    await step(ctx, 'T7 digits', () => tap(page, ...E.numTab), { wait: 1500 });
    await step(ctx, 'T7 type 2', () => tap(page, ...E.digit2), { wait: 1200, expect: 'change' });
    await step(ctx, 'T7 keyboard done', () => tap(page, ...E.keyboardDone), { wait: 2500, expect: 'change' });
    await step(ctx, 'T7 save edit', () => tap(page, ...E.formDone), { wait: 3000, expect: 'change' });
    await goldenStep(ctx, 'T7 after edit (golden)', GOLDEN.edited, await screen(page), ORG_ROWS);
    if (!(await reopenAddress(ctx, ref, base, 'T7 edited', GOLDEN.editedReopen, false))) return;
    if (!(await reopenAddress(ctx, ref, base, 'T7 edited relaunch', GOLDEN.editedReopen, true))) return;
    await step(ctx, 'T7 open 2てすと', () => tap(page, ...E.rowFirst), { wait: 3000, expect: 'change' });
    await step(ctx, 'T7 edit for delete', () => tap(page, ...E.edit), { wait: 3000, expect: 'change' });
    await step(ctx, 'T7 delete', () => tap(page, ...E.formDelete), { wait: 2500, expect: 'change' });
    await step(ctx, 'T7 confirm delete', () => tap(page, ...E.confirmOk), { wait: 3000, expect: 'change' });
    await goldenStep(ctx, 'T7 after delete (golden)', GOLDEN.deleted, await screen(page), ORG_ROWS);
    if (!(await reopenAddress(ctx, ref, base, 'T7 deleted', GOLDEN.deletedReopen, false))) return;
    if (!(await reopenAddress(ctx, ref, base, 'T7 deleted relaunch', GOLDEN.deletedReopen, true))) return;
    await backToLauncher(ctx, ref, 'T7 end home');
}
// home returns to the launcher; the launcher scrolls to show the application launched last, so the check allows
// up to three taps on its scroll-up arrow before comparing (no other input)
async function backToLauncher(ctx, ref, name) {
    const { page, s } = ctx; s.step = name;
    const failsBefore = s.fails.length;
    await tap(page, ...HOME); await sleep(5000);
    let cur = await screen(page), n = 0;
    while (diff(ref, cur, ...LAUNCHER_ROWS) > 0.01 && n < 3) { await tap(page, 313, 50); await sleep(1800); cur = await screen(page); n++; }
    const d = diff(ref, cur, ...LAUNCHER_ROWS);
    if (d > 0.01) s.fail('not back on the launcher (home did not return, or an application kept the screen)', `diff ${(d * 100).toFixed(1)}%`);
    s.checkPending();
    const status = s.fails.length > failsBefore ? 'NEW_FAILURE' : 'PASS';
    ctx.log.steps.push({ name, status, ok: status === 'PASS', ms: 0, note: `launcher after ${n} scroll-up taps, diff ${(d * 100).toFixed(2)}%` });
    console.log(`  ${status.padEnd(15)} ${name.padEnd(28)} launcher after ${n} scroll-up taps, diff ${(d * 100).toFixed(2)}%`);
    ctx.last = cur;
    if (status !== 'PASS') await artifacts(ctx, name, cur);
    return status === 'PASS';
}
function sha(file) { return require('crypto').createHash('sha256').update(fs.readFileSync(file)).digest('hex'); }
function buildMeta() {
    const dir = path.resolve(O.build);
    const wasm = fs.readdirSync(path.join(dir, 'wasm')).filter((f) => f.endsWith('.wasm')).sort();
    let rev = ''; try { rev = execSync('git rev-parse HEAD', { cwd: dir }).toString().trim(); } catch {}
    return { buildRevision: rev, wasm: Object.fromEntries(wasm.map((f) => [f, sha(path.join(dir, 'wasm', f)).slice(0, 16)])) };
}
const LAUNCHER = [[56, 62], [160, 62], [264, 62], [56, 130], [264, 130], [160, 198], [264, 198], [56, 266], [160, 266], [264, 266]];

// each launcher entry: start from the baseline launcher (scrolled to the top), launch, return home; stops when home
// does not lead back to the launcher (later taps would land in another application)
function notRun(ctx, names) {
    for (const name of names) { ctx.log.steps.push({ name, status: 'NOT_RUN', ok: false, ms: 0, note: '' }); console.log(`  ${'NOT_RUN'.padEnd(15)} ${name}`); }
}

async function appCycle(ctx, ref, tag) {
    const { page, s } = ctx;
    for (let i = 0; i < LAUNCHER.length; i++) {
        if (!(await ensureLauncher(ctx, ref, `${tag} app ${i + 1} launcher`))) { notRun(ctx, LAUNCHER.slice(i).map((_, k) => `${tag} app ${i + k + 1}`)); return false; }
        await step(ctx, `${tag} app ${i + 1} (${LAUNCHER[i]})`, () => tap(page, ...LAUNCHER[i]), { wait: 7000, expect: 'change', ref, rows: LAUNCHER_ROWS });
        await step(ctx, `${tag} app ${i + 1} home`, () => tap(page, ...HOME), { wait: 5000 });
    }
    return !s.fails.length && (await ensureLauncher(ctx, ref, `${tag} end launcher`));
}

// T3: hard buttons, rocker, rapid input, and one function inside an application (Calculator, golden image)
async function inputTests(ctx, ref, work) {
    const { page, s } = ctx;
    const rest = (from) => notRun(ctx, ['T3 rocker', 'T3 rapid rocker x20', 'T3 calculator'].slice(from));
    for (const [name, xy] of Object.entries(BUTTONS)) {
        if (!(await ensureLauncher(ctx, ref, `T3 ${name} launcher`))) return rest(0);
        await step(ctx, `T3 button ${name}`, () => tap(page, ...xy), { wait: 6000, expect: 'change', ref, rows: LAUNCHER_ROWS });
        await step(ctx, `T3 button ${name} home`, () => tap(page, ...HOME), { wait: 5000 });
    }
    if (!(await ensureLauncher(ctx, ref, 'T3 rocker launcher'))) return rest(0);
    await step(ctx, 'T3 rocker down', () => tap(page, ...ROCKER_DOWN), { wait: 2500, expect: 'change' });
    await step(ctx, 'T3 rocker up', () => tap(page, ...ROCKER_UP), { wait: 2500, expect: 'change' });
    await step(ctx, 'T3 rapid rocker x20', async () => {
        for (let i = 0; i < 20; i++) { const [x, y] = i % 2 ? ROCKER_UP : ROCKER_DOWN; const g = await geometry(page); const p = toPage(g, x, y);
            await page.mouse.move(p.x, p.y); await page.mouse.down(); await sleep(60); await page.mouse.up(); await sleep(60); }
    }, { wait: 3000 });
    if (!(await ensureLauncher(ctx, ref, 'T3 after rapid input'))) return rest(2);
    // Calculator: C 1 2 + 3 4 = ; the display row is compared with the golden image of the first run
    await step(ctx, 'T3 open Calculator', () => tap(page, 160, 198), { wait: 6000, expect: 'change', ref, rows: LAUNCHER_ROWS });
    s.step = 'T3 calculator input';
    for (const k of ['C', 1, 2, 'plus', 3, 4, 'eq']) { await tap(page, ...CALC[k]); await sleep(700); }
    await sleep(1500);
    const cur = await screen(page); const golden = path.join(work, 'golden-calc.raw');
    // independent of the golden image: the result of 12+34= must look exactly like 46 typed in directly, and not
    // like the cleared display
    s.step = 'T3 calculator check'; await tap(page, ...CALC.C); await sleep(1200); const cleared = await screen(page);
    await tap(page, ...CALC[4]); await sleep(700); await tap(page, ...CALC[6] || [155, 232]); await sleep(1500);
    const typed = await screen(page);
    const dRes = diff(cur, typed, ...CALC_DISPLAY_ROWS), dClr = diff(cur, cleared, ...CALC_DISPLAY_ROWS);
    const calcOk = dRes <= 0.002 && dClr > 0.002;
    ctx.log.steps.push({ name: 'T3 calculator 12+34= equals typed 46', status: calcOk ? 'PASS' : 'NEW_FAILURE', ok: calcOk, ms: 0, note: `vs 46 ${(dRes * 100).toFixed(2)}%, vs cleared ${(dClr * 100).toFixed(2)}%` });
    if (!(dRes <= 0.002 && dClr > 0.002)) s.fail('calculator result is not 46', `vs typed 46 ${(dRes * 100).toFixed(2)}%, vs cleared ${(dClr * 100).toFixed(2)}%`);
    console.log(`  ${(calcOk ? 'PASS' : 'NEW_FAILURE').padEnd(15)} T3 calculator 12+34= equals typed 46  vs 46 ${(dRes * 100).toFixed(2)}%, vs cleared ${(dClr * 100).toFixed(2)}%`);
    fs.writeFileSync(path.join(ctx.log.dir, 'calc-result.png'), cur.png); fs.writeFileSync(path.join(ctx.log.dir, 'calc-typed46.png'), typed.png);
    // golden image: compared only after a person has looked at golden-calc.png and confirmed it shows 46
    // ("verify-golden calc"). The manifest ties it to its SHA-256 and to the input coordinates it was made with.
    await goldenStep(ctx, 'T3 calculator 12+34= (golden)', GOLDEN.calc, cur, CALC_DISPLAY_ROWS);
    await step(ctx, 'T3 calculator home', () => tap(page, ...HOME), { wait: 5000 });
    await ensureLauncher(ctx, ref, 'T3 end launcher');
}

// classification self-test without an emulator: synthetic console lines through a Session
async function selftest() {
    const EventEmitter = require('events');
    const run = async (name, step, lines, waitMs = 0) => {
        const page = new EventEmitter(); const s = new Session(page, { t0: now() }); s.step = step;
        for (const l of lines) page.emit('console', { type: () => 'log', text: () => l });
        if (waitMs) { await sleep(waitMs); s.checkPending(1000); }
        return { name, fails: s.fails.map((f) => f.what), known: s.known.map((k) => k.known) };
    };
    const F = (pc, op = '4212', addr = '00000000') => `0: [diag] pace-fault w addr=${addr} pc=10d2da74 at=rom pc68k=${pc} op=${op}`;
    const R = (r1, r2) => `0: [diag] pace-frames a6=207e22f4 ret=${r1} a6=207e2324 ret=${r2} a6=207e2416 ret=00000000`;
    const cases = [
        [await run('known signature (rebuilt ROM)', 'K memo button', [F('10877fc4'), R('108764fc', '108740fa')]), { known: 1, fails: 0 }],
        [await run('known signature (reference ROM layout)', 'K memo button', [F('10457fe4'), R('1045651c', '1045411a')]), { known: 1, fails: 0 }],
        [await run('other PACE fault (different opcode)', 'K memo button', [F('10877fc4', '4290'), R('108764fc', '108740fa')]), { known: 0, fails: 1 }],
        [await run('other PACE fault (different call path)', 'K memo button', [F('10877fc4'), R('10876400', '108740fa')]), { known: 0, fails: 1 }],
        [await run('known signature outside its step', 'T2 app 3', [F('10877fc4'), R('108764fc', '108740fa')]), { known: 0, fails: 1 }],
        [await run('frame line missing', 'K memo button', [F('10877fc4')], 1300), { known: 0, fails: 1 }],
        [await run('storage error after the known fault', 'K memo button home', [F('10877fc4'), R('108764fc', '108740fa'),
            "error InvalidStateError: Failed to execute 'transaction' on 'IDBDatabase': The database connection is closing."]), { known: 1, fails: 1 }],
        [await run('service worker refused by the test server', 'T0 restore', ["error SecurityError: Failed to register a ServiceWorker for scope ('http://127.0.0.1:8790/') with script ('http://127.0.0.1:8790/ngsw-worker.js'): The script has an unsupported MIME type ('text/html')."]), { known: 0, fails: 0 }],
    ];
    let ok = true;
    for (const [r, want] of cases) {
        const pass = r.known.length === want.known && r.fails.length === want.fails; ok &&= pass;
        console.log(`  ${pass ? 'PASS' : 'FAIL'} ${r.name.padEnd(40)} known=${r.known.length} new=${r.fails.length} ${r.fails.join('; ')}`);
    }
    return ok;
}

async function soak() {
    const work = path.resolve(O.work); const base = await checkBaseline(work);
    const ref = { data: fs.readFileSync(path.join(work, 'baseline-screen.raw')) };
    const server = await serve(path.resolve(O.build)); const url = `http://127.0.0.1:${server.address().port}/`;
    const log = { t0: now(), steps: [], dir: path.join(work, `${O.label}-soak`), loops: [] };
    fs.rmSync(log.dir, { recursive: true, force: true }); fs.mkdirSync(log.dir, { recursive: true });
    const profile = path.join(work, `${O.label}-profile`); fs.rmSync(profile, { recursive: true, force: true });
    fs.cpSync(path.join(work, 'baseline-profile'), profile, { recursive: true });
    const { ctx: bctx, page, s } = await openBrowser(profile, url, log);
    const ctx = { page, s, log };
    s.step = 'launch'; await launchSession(page, base.name);
    await step(ctx, 'restore = baseline', null, { wait: 8000, expect: 'same', ref, tolerance: 0.01, rows: LAUNCHER_ROWS });
    for (let l = 1; l <= +O.loops && !s.fails.length; l++) {
        const t = now();
        await appCycle(ctx, ref, `loop ${l}`);
        const last = s.summaries[s.summaries.length - 1] || {};
        const m = { loop: l, ms: now() - t, rssMB: rendererRssMB(), heapMiB: last.heapMiB, ips: last.ips };
        log.loops.push(m); console.log('  loop', JSON.stringify(m));
    }
    await bctx.close(); server.close();
    const r = report(log, s, `${O.label} soak`); r.loops = log.loops;
    fs.writeFileSync(path.join(log.dir, 'result.json'), JSON.stringify(r, null, 1));
    return r.failures.length === 0;
}

async function setup() {
    const work = path.resolve(O.work); fs.mkdirSync(work, { recursive: true });
    const log = { t0: now(), steps: [], dir: path.join(work, 'setup') }; fs.rmSync(log.dir, { recursive: true, force: true }); fs.mkdirSync(log.dir, { recursive: true });
    const server = await serve(path.resolve(O.build)); const url = `http://127.0.0.1:${server.address().port}/`;
    const profile = path.join(work, 'setup-profile'); fs.rmSync(profile, { recursive: true, force: true });
    const { ctx: bctx, page, s } = await openBrowser(profile, url, log);
    const ctx = { page, s, log };
    // register: ROM -> DAL dialog -> bundle -> settings -> save
    s.step = 'register';
    await tabSessions(page);
    let [fc] = await Promise.all([page.waitForEvent('filechooser'), page.locator('ion-button:has(ion-icon[name="add-outline"])').click()]);
    await fc.setFiles(O.rom); await sleep(2500);
    [fc] = await Promise.all([page.waitForEvent('filechooser'), page.locator('ion-alert button', { hasText: 'Select DAL bundle' }).click()]);
    await fc.setFiles(O.dal); await sleep(6000);
    await page.locator('ion-modal ion-button', { hasText: 'Save' }).click(); await sleep(3000);
    const name = path.basename(O.rom);
    // boot: time until the screen shows something and stays stable
    s.step = 'boot'; const tBoot = now();
    await launchSession(page, name);
    // booted when the Welcome screen's header tab (light grey, device x 2..40, y 2..12) is drawn
    let prev = null, bootMs = null;
    for (let i = 0; i < 120 && bootMs === null; i++) {
        await sleep(1000); const cur = await screen(page); prev = cur;
        if (hasHeaderTab(cur)) bootMs = now() - tBoot;
    }
    log.bootMs = bootMs; console.log(`  boot to Welcome: ${bootMs === null ? 'not reached' : (bootMs / 1000).toFixed(1) + ' s'}`);
    if (bootMs === null) s.fail('boot: Welcome screen not reached in 120 s', '');
    s.step = 'settle after boot';
    await sleep(15000); prev = await screen(page);                       // let the Welcome application settle
    ctx.last = prev;
    // T1: Welcome, driven by what is on the screen
    let n = 0;
    for (let i = 0; i < 4 && !calibrationTarget(ctx.last) && !s.fails.length; i++)          // A: text screens
        await step(ctx, `T1 welcome text ${++n}`, () => tap(page, 160, 200), { wait: 5000 });
    if (!calibrationTarget(ctx.last)) s.fail('no response: calibration screen not reached after 4 taps', '');
    for (let i = 0; i < 8 && !s.fails.length; i++) {                                          // B: calibration targets
        const t = calibrationTarget(ctx.last); if (!t) break;
        await step(ctx, `T1 calibrate ${t.where} (${t.x},${t.y})`, () => tap(page, t.x, t.y), { wait: 4000, expect: 'change' });
    }
    if (!calibrationTarget(ctx.last))
        for (const [x, y] of WELCOME_AFTER_CALIBRATION) await step(ctx, `T1 welcome ${++n} (${x},${y})`, () => tap(page, x, y), { wait: 6000, expect: 'change' });
    // Home goes to the Media Launcher by default: unassign it (Options > Media Launcher settings), as in the native runs
    await step(ctx, 'T1 menu', () => tap(page, ...MENU), { wait: 3000, expect: 'change' });
    await step(ctx, 'T1 options', () => tap(page, 145, 14), { wait: 3000, expect: 'change' });
    await step(ctx, 'T1 Media Launcher settings', () => tap(page, 150, 72), { wait: 4000, expect: 'change' });
    await step(ctx, 'T1 unassign Home', () => tap(page, 52, 56), { wait: 2000, expect: 'change', tolerance: 0.0001 });   // checkbox only
    await step(ctx, 'T1 OK', () => tap(page, 60, 297), { wait: 4000, expect: 'change' });
    await step(ctx, 'T1 Home -> launcher', () => tap(page, ...HOME), { wait: 6000, expect: 'change' });
    await step(ctx, 'T1 settle', null, { wait: 6000 });
    fs.writeFileSync(path.join(work, 'baseline-screen.png'), ctx.last.png);
    fs.writeFileSync(path.join(work, 'baseline-screen.raw'), ctx.last.data);
    await tabSessions(page); await sleep(6000);              // leaving the emulator saves the session
    await bctx.close(); server.close();
    const base = path.join(work, 'baseline-profile'); fs.rmSync(base, { recursive: true, force: true });
    fs.cpSync(profile, base, { recursive: true });
    const meta = { name, bootMs, created: new Date().toISOString(), romMd5: require('crypto').createHash('md5').update(fs.readFileSync(O.rom)).digest('hex'),
        dalSha256: sha(O.dal), baselineScreenSha256: sha(path.join(work, 'baseline-screen.png')), browser: chromium.name() + ' ' + (await browserVersion()),
        setupPassed: s.fails.length === 0, ...buildMeta() };
    fs.writeFileSync(path.join(work, 'baseline.json'), JSON.stringify(meta, null, 1));
    report(log, s, 'setup');
    return s.fails.length === 0;
}

async function browserVersion() { const b = await chromium.launch(); const v = b.version(); await b.close(); return v; }

// a baseline is only used with the build, ROM, DAL and browser it was made with, and only if its setup passed
async function checkBaseline(work) {
    const base = JSON.parse(fs.readFileSync(path.join(work, 'baseline.json')));
    const now_ = buildMeta(); const v = chromium.name() + ' ' + (await browserVersion());
    const problems = [];
    if (!base.setupPassed) problems.push('setup of this baseline did not pass');
    if (JSON.stringify(base.wasm) !== JSON.stringify(now_.wasm)) problems.push('wasm files differ from the baseline build');
    if (base.browser !== v) problems.push(`browser ${v} differs from ${base.browser}`);
    if (sha(path.join(work, 'baseline-screen.png')) !== base.baselineScreenSha256) problems.push('baseline screen changed');
    if (problems.length) { console.log('baseline refused:\n  ' + problems.join('\n  ')); process.exit(2); }
    BASELINE_ID = sha(path.join(work, 'baseline.json')).slice(0, 16);
    return base;
}

// explore: interactive session for designing new steps. Restores the baseline, then executes the lines appended to
// <work>/explore.cmd ("tap x y" in device units, "type text", "press Key", "wait ms", "shot name", "quit"), writing
// each screenshot to <work>/explore/<n>-<name>.png and "ok <n> <command>" to stdout. Records nothing as a result.
async function explore() {
    const work = path.resolve(O.work); const base = await checkBaseline(work);
    const server = await serve(path.resolve(O.build)); const url = `http://127.0.0.1:${server.address().port}/`;
    const log = { t0: now(), steps: [], dir: path.join(work, 'explore') }; fs.rmSync(log.dir, { recursive: true, force: true }); fs.mkdirSync(log.dir, { recursive: true });
    const profile = path.join(work, 'explore-profile'); fs.rmSync(profile, { recursive: true, force: true });
    fs.cpSync(path.join(work, 'baseline-profile'), profile, { recursive: true });
    const { ctx: bctx, page, s } = await openBrowser(profile, url, log);
    await launchSession(page, base.name); await sleep(8000);
    const cmdFile = path.join(work, 'explore.cmd'); fs.writeFileSync(cmdFile, '');
    let done = 0, n = 0;
    console.log('explore ready');
    for (;;) {
        const lines = fs.readFileSync(cmdFile, 'utf8').split('\n').filter((l) => l.trim());
        if (done >= lines.length) { await sleep(300); continue; }
        const line = lines[done++], [cmd, ...rest] = line.trim().split(/\s+/);
        if (cmd === 'quit') break;
        if (cmd === 'tap') await tap(page, +rest[0], +rest[1]);
        else if (cmd === 'type') await page.keyboard.type(rest.join(' '), { delay: 120 });
        else if (cmd === 'press') await page.keyboard.press(rest[0]);
        else if (cmd === 'wait') await sleep(+rest[0]);
        else if (cmd === 'fullshot') {     // the whole canvas, status bar included (device rows 0..480)
            const g = await geometry(page), a = toPage(g, -1, -1), z = toPage(g, 321, 481);
            fs.writeFileSync(path.join(log.dir, `${String(++n).padStart(2, '0')}-${rest[0] || 'full'}.png`), await page.screenshot({ clip: { x: a.x, y: a.y, width: z.x - a.x, height: z.y - a.y } }));
        }
        else if (cmd === 'shot') { const sc = await screen(page); fs.writeFileSync(path.join(log.dir, `${String(++n).padStart(2, '0')}-${rest[0] || 'shot'}.png`), sc.png); }
        console.log(`ok ${done} ${line}`);
        const f = s.fails.splice(0); for (const x of f) console.log(`  event ${x.what} ${x.detail.slice(0, 120)}`);
    }
    fs.writeFileSync(path.join(log.dir, 'console.txt'), s.lines.join('\n'));
    await bctx.close(); server.close(); return true;
}

async function rounds() {
    const work = path.resolve(O.work); const base = await checkBaseline(work);
    const ref = { data: fs.readFileSync(path.join(work, 'baseline-screen.raw')) };
    const server = await serve(path.resolve(O.build)); const url = `http://127.0.0.1:${server.address().port}/`;
    const all = [];
    for (let r = 1; r <= +O.rounds; r++) {
        const log = { t0: now(), steps: [], dir: path.join(work, `${O.label}-round${r}`) }; fs.rmSync(log.dir, { recursive: true, force: true }); fs.mkdirSync(log.dir, { recursive: true });
        const profile = path.join(work, `${O.label}-profile`); fs.rmSync(profile, { recursive: true, force: true });
        fs.cpSync(path.join(work, 'baseline-profile'), profile, { recursive: true });
        console.log(`round ${r}`);
        const { ctx: bctx, page, s } = await openBrowser(profile, url, log);
        const ctx = { page, s, log };
        const rss0 = rendererRssMB();
        s.step = 'T0 restore'; const tL = now();
        await launchSession(page, base.name);
        await step(ctx, 'T0 restore = baseline', null, { wait: 8000, expect: 'same', ref, tolerance: 0.01, rows: LAUNCHER_ROWS });
        log.restoreMs = now() - tL;
        // T2: Prefs -> General -> back home; then launch each launcher entry and return home
        await step(ctx, 'T2 open Prefs', () => tap(page, ...PREFS), { wait: 6000, expect: 'change' });
        await step(ctx, 'T2 panel menu', () => tap(page, 300, 12), { wait: 3000, expect: 'change' });
        await step(ctx, 'T2 General', () => tap(page, 230, 145), { wait: 4000, expect: 'change' });
        await step(ctx, 'T2 home', () => tap(page, ...HOME), { wait: 5000, expect: 'change' });
        await appCycle(ctx, ref, 'T2');
        if (!s.fails.length) await inputTests(ctx, ref, work);
        // T4: leave (saves) and relaunch: same screen
        const beforeSave = await screen(page);
        s.step = 'T4 save'; await tabSessions(page); await sleep(5000);
        await launchSession(page, base.name);
        await step(ctx, 'T4 relaunch = before', null, { wait: 6000, expect: 'same', ref: beforeSave, tolerance: 0.01 });
        await step(ctx, 'T4 still answers', () => tap(page, ...PREFS), { wait: 6000, expect: 'change' });
        // T5: Memo hard button (formerly the known fault memo-button-pace-null-write: CLIE Organizer writes one byte to
        // address 0; fixed by RAM at physical 0 on VZ90 images, emulator commit 13a12502). Kept last: if it regresses,
        // the emulator is left unusable for this round.
        if (!(await ensureLauncher(ctx, ref, 'T5 launcher'))) notRun(ctx, ['T5 memo button']);
        else {
            await step(ctx, 'T5 memo button', () => tap(page, ...MEMO_BUTTON), { wait: 8000, expect: 'change', ref, rows: LAUNCHER_ROWS, tolerance: 0.3 });
            await goldenStep(ctx, 'T5 memo button = CLIE Organizer (golden)', GOLDEN.org, await screen(page), ORG_ROWS);
            if (await backToLauncher(ctx, ref, 'T5 memo button home')) await organizerEntry(ctx, ref, base);
        }
        log.rssMB = [rss0, rendererRssMB()];
        await bctx.close();
        all.push(report(log, s, `${O.label} round ${r}`));
    }
    server.close();
    fs.writeFileSync(path.join(work, `${O.label}-summary.json`), JSON.stringify(all, null, 1));
    return all.every((x) => x.failures.length === 0);
}

// register census from the diag build: per device, registers seen (r/w), refused ones called out
function mmioTable(list) {
    const dev = {};
    for (const x of list) {
        const d = (dev[x.dev] = dev[x.dev] || { regs: new Set(), refused: [], fromRom: new Set() });
        d.regs.add(`${x.rw}:${x.addr}`);
        if (!x.ok) d.refused.push(`${x.rw}:${x.addr} pc ${x.pc} (${x.at}) during "${x.step}"`);
        if (x.at !== 'dal') d.fromRom.add(`${x.rw}:${x.addr} pc ${x.pc} (${x.at})`);
    }
    const out = { devices: {} };
    for (const [k, v] of Object.entries(dev)) out.devices[k] = { registers: [...v.regs].sort(), refused: v.refused, notFromDal: [...v.fromRom] };
    // keys are (address, direction); addresses count each location once. PV device areas vs special memory
    // (Sony shared page, exception vectors in tinyram) are counted apart: only the former are device registers
    const special = (d) => d === 'sony-shared' || d === 'tinyram';
    for (const [name, pick] of [['pvDevices', (d) => !special(d)], ['specialMemory', special]]) {
        const keys = list.filter((x) => pick(x.dev));
        out[name] = { accessKeys: new Set(keys.map((x) => x.rw + x.addr)).size, addresses: new Set(keys.map((x) => x.addr)).size };
    }
    return out;
}

function report(log, s, title) {
    const sums = s.summaries;
    const ips = sums.filter((x) => x.ips > 0).map((x) => x.ips);
    const r = {
        title, steps: log.steps.length, diskFreeGiB: log.diskFreeGiB || [],
        status: log.steps.reduce((a, x) => ((a[x.status || (x.ok ? 'PASS' : 'NEW_FAILURE')] = (a[x.status || (x.ok ? 'PASS' : 'NEW_FAILURE')] || 0) + 1), a), {}),
        failed: log.steps.filter((x) => x.status === 'NEW_FAILURE').map((x) => x.name),
        failures: s.fails.map((f) => `${f.step}: ${f.what}${f.detail ? ' | ' + f.detail : ''}`),
        totalMs: now() - log.t0, bootMs: log.bootMs, restoreMs: log.restoreMs, rssMB: log.rssMB,
        diag: sums.length ? { summaries: sums.length, ipsMedian: ips.sort((a, b) => a - b)[Math.floor(ips.length / 2)] || 0, heapMiB: [sums[0].heapMiB, sums[sums.length - 1].heapMiB], aborts: sums[sums.length - 1].aborts, uniqueSites: sums[sums.length - 1].unique, undef: sums[sums.length - 1].undef } : 'no diag summaries (regular build?)',
        abortSites: [...s.sites.values()],
        mmio: mmioTable(s.mmio),
        sharedPageWrites: s.shared,
        known: s.known.map((k) => `${k.known}: ${k.step}: ${k.what}`),
        paceFaults: s.pace.map((p) => ({ ...p, pc68k: p.pc68k.toString(16), d1: p.d1 != null ? p.d1.toString(16) : null, d2: p.d2 != null ? p.d2.toString(16) : null })),
        baselineId: BASELINE_ID,
    };
    // suspected origin of the first failure: the diag events (aborts, undefined instructions) seen before it
    if (s.fails.length) {
        const first = s.fails[0];
        r.firstFailure = `${first.step}: ${first.what}`;
        // (+2 s: the emulator's own message about an event can reach the console just before the diag line)
        r.suspects = [...s.sites.values()].filter((x) => x.t <= first.at + 2000).slice(-3)
            .map((x) => `${x.kind}${x.rw ? ' ' + x.rw : ''} ${x.addr ? 'addr ' + x.addr + ' (' + x.region + ') ' : ''}pc ${x.pc} in ${x.at}, during "${x.step}"`);
        console.log(`  first failure: ${r.firstFailure}`);
        for (const x of r.suspects) console.log(`  suspected origin: ${x}`);
        if (!r.suspects.length) console.log('  suspected origin: no diag event before it');
    }
    fs.writeFileSync(path.join(log.dir, 'result.json'), JSON.stringify(r, null, 1));
    fs.writeFileSync(path.join(log.dir, 'console.txt'), s.lines.join('\n'));
    fs.writeFileSync(path.join(log.dir, 'diag.txt'), s.diag.join('\n'));
    console.log(`  => ${title}: ${r.failures.length ? 'NEW_FAILURE ' + r.failures.length : 'no new failure'} ${JSON.stringify(r.status)}`, JSON.stringify(r.diag));
    return r;
}

(async () => {
    const ok = MODE === 'setup' ? await setup() : MODE === 'rounds' ? await rounds() : MODE === 'soak' ? await soak()
        : MODE === 'selftest' ? await selftest() : MODE === 'verify-golden' ? verifyGolden() : MODE === 'explore' ? await explore()
        : (console.log('mode: setup | rounds | soak | selftest | verify-golden'), false);
    process.exit(ok ? 0 : 1);
})();
