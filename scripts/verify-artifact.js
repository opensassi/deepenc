#!/usr/bin/env node
const path = require('path');
const fs = require('fs');
const http = require('http');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..');

function serveStatic(dir) {
  const server = http.createServer((req, res) => {
    const filePath = path.join(dir, req.url === '/' ? 'index.html' : req.url);
    if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    const ext = path.extname(filePath);
    const mime = { '.html': 'text/html', '.js': 'application/javascript', '.css': 'text/css', '.png': 'image/png', '.svg': 'image/svg+xml' };
    res.writeHead(200, { 'Content-Type': mime[ext] || 'application/octet-stream' });
    fs.createReadStream(filePath).pipe(res);
  });
  return new Promise((resolve) => {
    server.listen(0, '127.0.0.1', () => resolve(server));
  });
}

function parseArgs() {
  const args = process.argv.slice(2);
  const fileIdx = args.indexOf('--file');
  if (fileIdx === -1) {
    console.error('Usage: node scripts/verify-artifact.js --file <path-to-d3-animation.html>');
    process.exit(1);
  }
  return { filePath: args[fileIdx + 1] };
}

async function verify() {
  const { filePath } = parseArgs();
  const absPath = path.resolve(ROOT, filePath);
  if (!fs.existsSync(absPath)) {
    console.error(`ERROR: File not found: ${absPath}`);
    process.exit(1);
  }

  const dir = path.dirname(absPath);
  const fileName = path.basename(absPath);
  const rel = path.relative(ROOT, absPath);

  console.log(`Verifying ${rel}...\n`);

  const server = await serveStatic(dir);
  const port = server.address().port;
  const url = `http://127.0.0.1:${port}/${fileName}`;

  let browser;
  const results = { pass: 0, fail: 0, total: 0, details: [] };

  try {
    browser = await chromium.launch({ headless: true });
    const page = await browser.newPage({ viewport: { width: 720, height: 480 } });

    await page.goto(url, { waitUntil: 'networkidle', timeout: 15000 });

    // Check required globals
    const duration = await page.evaluate(() => window.ANIMATION_DURATION_MS);
    const keyframes = await page.evaluate(() => window.ANIMATION_KEYFRAMES);
    const verification = await page.evaluate(() => window.ANIMATION_VERIFICATION);
    const hasPlayPause = await page.evaluate(() => !!document.querySelector('[data-testid="play-pause"]'));

    if (!duration || duration <= 0) {
      console.error('  ✗ ANIMATION_DURATION_MS not set or invalid');
      process.exit(1);
    }
    if (!keyframes || !keyframes.length) {
      console.error('  ✗ ANIMATION_KEYFRAMES not set or empty');
      process.exit(1);
    }
    if (!verification || verification.length !== keyframes.length) {
      console.error('  ✗ ANIMATION_VERIFICATION missing or length mismatch');
      process.exit(1);
    }
    if (!hasPlayPause) {
      console.error('  ✗ [data-testid="play-pause"] not found');
      process.exit(1);
    }
    console.log(`  ✓ Globals: duration=${duration}ms, keyframes=${keyframes.length}, [data-testid="play-pause"] found\n`);

    // Verify each keyframe
    for (let i = 0; i < verification.length; i++) {
      const expected = verification[i];

      await page.evaluate((idx) => window.jumpToKeyframe(idx), i);

      const state = await page.evaluate(() => window.getAnimationState());

      const assertions = [
        { field: 'hor',         actual: state.hor,              expected: expected.hor },
        { field: 'ver',         actual: state.ver,              expected: expected.ver },
        { field: 'precision',   actual: state.precision,        expected: expected.precision },
        { field: 'logCount',    actual: state.logCount,         expected: expected.logCount },
      ];

      if (expected.bounds) {
        assertions.push({ field: 'boundsOpacity', actual: state.boundsOpacity, expected: '1' });
      }

      const failures = assertions.filter(a => String(a.actual) !== String(a.expected));
      const passed = failures.length === 0;

      if (passed) {
        results.pass++;
        console.log(`  ✓ [${i}] ${expected.label.padEnd(20)} hor=${state.hor} ver=${state.ver} prec=${state.precision} log=${state.logCount}`);
      } else {
        results.fail++;
        console.error(`  ✗ [${i}] ${expected.label}:`);
        for (const f of failures) {
          console.error(`       ${f.field}: expected ${f.expected}, got ${f.actual}`);
        }
      }
      results.total++;
      results.details.push({ keyframe: i, label: expected.label, passed, failures: failures.map(f => f.field) });
    }

    // Check play/pause toggle
    await page.evaluate(() => window.resetAnimation());
    const playBtnTextBefore = await page.evaluate(() => document.querySelector('[data-testid="play-pause"]').textContent);
    await page.click('[data-testid="play-pause"]');
    await new Promise(r => setTimeout(r, 200));
    const playBtnTextAfter = await page.evaluate(() => document.querySelector('[data-testid="play-pause"]').textContent);
    const toggleOk = playBtnTextBefore !== playBtnTextAfter;
    if (toggleOk) {
      console.log(`  ✓ Play/Pause toggle works: "${playBtnTextBefore.trim()}" → "${playBtnTextAfter.trim()}"`);
    } else {
      console.error(`  ✗ Play/Pause toggle failed: text did not change ("${playBtnTextBefore.trim()}")`);
    }

    console.log(`\n=== Results ===`);
    console.log(`  ${results.pass}/${results.total} keyframes passed`);
    if (results.fail > 0) {
      console.error(`  ${results.fail} keyframe(s) FAILED`);
      process.exit(1);
    }
    console.log(`  Play/Pause toggle: ${toggleOk ? '✓' : '✗'}`);
    console.log(`\n✓ All assertions passed.`);

  } catch (e) {
    console.error('Fatal error:', e.message);
    process.exit(1);
  } finally {
    if (browser) await browser.close();
    server.close();
  }
}

verify();
