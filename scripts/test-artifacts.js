#!/usr/bin/env node
const { chromium } = require('playwright');
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');

const ROOT = path.resolve(__dirname, '..');

function findArtifactDirs(specPath) {
  // When --file is given, return only that spec's artifact directory
  if (specPath) {
    const absSpec = path.resolve(ROOT, specPath);
    const specDir = path.dirname(absSpec);
    const specName = path.basename(absSpec);
    const artifactDir = path.join(specDir, '.artifacts', specName);
    if (fs.existsSync(artifactDir)) {
      const label = specPath.replace(/\.spec\.md$/, '');
      return { [label]: artifactDir };
    }
    console.error(`  No artifact directory found for ${specPath} (expected ${artifactDir})`);
    return {};
  }

  const dirs = {};

  const rootArtifacts = path.join(ROOT, '.artifacts');
  if (fs.existsSync(rootArtifacts)) dirs['.artifacts'] = rootArtifacts;

  for (const base of ['src', 'source']) {
    const baseDir = path.join(ROOT, base);
    if (!fs.existsSync(baseDir)) continue;

    const xFind = (dir, label) => {
      const artifactPath = path.join(dir, '.artifacts');
      if (fs.existsSync(artifactPath)) {
        dirs[label] = artifactPath;
      }
      for (const sub of fs.readdirSync(dir, { withFileTypes: true })) {
        if (sub.isDirectory() && sub.name !== '.artifacts' && sub.name !== 'node_modules') {
          xFind(path.join(dir, sub.name), `${label}/${sub.name}`);
        }
      }
    };

    for (const entry of fs.readdirSync(baseDir, { withFileTypes: true })) {
      if (entry.isDirectory()) {
        xFind(path.join(baseDir, entry.name), `${base}/${entry.name}`);
      }
    }
  }

  return dirs;
}

function findFilesRecursive(dir, ext) {
  const results = [];
  if (!fs.existsSync(dir)) return results;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      results.push(...findFilesRecursive(fullPath, ext));
    } else if (entry.name.endsWith(ext)) {
      results.push(fullPath);
    }
  }
  return results;
}

async function validateMermaid(specPath) {
  const results = { pass: [], fail: [] };
  const dirs = findArtifactDirs(specPath);

  let totalFound = 0;
  for (const [label, dir] of Object.entries(dirs)) {
    const mmdFiles = findFilesRecursive(dir, '.mmd');
    totalFound += mmdFiles.length;
    for (const mmdFile of mmdFiles) {
      const pngFile = mmdFile.replace(/\.mmd$/, '.png');
      const rel = path.relative(ROOT, mmdFile);
      try {
        const puppeteerConfig = path.join(ROOT, 'scripts', 'puppeteer-config.json');
        execSync(`mmdc -i "${mmdFile}" -o "${pngFile}" -b transparent -p "${puppeteerConfig}"`, { stdio: 'pipe', timeout: 30000 });
        results.pass.push({ file: rel, png: path.relative(ROOT, pngFile) });
        console.log(`  ✓ ${rel}`);
      } catch (e) {
        const stderr = (e.stderr || '').toString() || e.message;
        results.fail.push({ file: rel, error: stderr.substring(0, 300) });
        console.error(`  ✗ ${rel}: ${stderr.substring(0, 100)}`);
      }
    }
  }

  if (totalFound === 0) {
    console.log('  (no .mmd files found — run `npm run extract` first)');
  }

  return results;
}

async function captureFilmstrip(htmlFile) {
  const browser = await chromium.launch({ headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 720, height: 480 } });
    await page.goto(`file://${htmlFile}`, { waitUntil: 'networkidle', timeout: 30000 });

    const duration = await page.evaluate(() => {
      if (typeof window.ANIMATION_DURATION_MS !== 'undefined') return window.ANIMATION_DURATION_MS;
      try {
        const meta = document.querySelector('meta[name="animation-duration-ms"]');
        if (meta) return parseInt(meta.getAttribute('content'), 10);
      } catch {}
      return null;
    });

    const keyframes = await page.evaluate(() => {
      if (typeof window.ANIMATION_KEYFRAMES !== 'undefined' && Array.isArray(window.ANIMATION_KEYFRAMES) && window.ANIMATION_KEYFRAMES.length > 0) {
        return window.ANIMATION_KEYFRAMES.map(kf => ({ time: kf.time, label: kf.label }));
      }
      return null;
    });

    if (!keyframes) {
      if (!duration || duration <= 0) {
        return { pass: false, error: 'ANIMATION_KEYFRAMES not set in page' };
      }
    }

    const filmDir = path.join(path.dirname(htmlFile), 'd3-animation-filmstrip');
    fs.mkdirSync(filmDir, { recursive: true });

    const frames = [];

    if (keyframes) {
      for (let i = 0; i < keyframes.length; i++) {
        const kf = keyframes[i];
        await page.evaluate((idx) => {
          if (typeof window.jumpToKeyframe === 'function') {
            window.jumpToKeyframe(idx);
          } else {
            // Fallback: seek via keyframes time if no jumpToKeyframe
            const allKf = window.ANIMATION_KEYFRAMES || [];
            const at = allKf[idx] ? allKf[idx].time : 0;
            if (window.__seekToTime) window.__seekToTime(at);
          }
        }, i);
        const settle = i === 0 ? 1000 : 200;
        await new Promise(r => setTimeout(r, settle));
        const domState = await page.evaluate(() => {
          if (typeof window.getAnimationState === 'function') {
            const s = window.getAnimationState();
            return `hor=${s.hor} ver=${s.ver} prec=${s.precision} log=${s.logCount}`;
          }
          return '';
        });
        const frameFile = path.join(filmDir, `frame-${i}-${kf.label}.png`);
        const tmpFile = path.join(filmDir, `frame-${i}-${kf.label}.tmp.png`);
        await page.screenshot({ path: tmpFile });
        const image = sharp(tmpFile);
        const meta = await image.metadata();
        if (meta.width !== 720) {
          image.resize({ width: 720, fit: 'inside', withoutEnlargement: true });
        }
        await image.greyscale().toFile(frameFile);
        fs.unlinkSync(tmpFile);
        execSync(`convert "${frameFile}" -colorspace Gray "${frameFile}"`, { stdio: 'ignore' });
        frames.push({ file: path.relative(ROOT, frameFile), label: kf.label, time: kf.time });
        console.log(`    frame ${i}/${keyframes.length} → frame-${i}-${kf.label}.png  [${domState}]`);
      }
      return { pass: true, animationDurationMs: duration || keyframes[keyframes.length - 1].time, frames, keyframes, skipped: false };
    }

    // Fallback: fixed 5-fraction capture
    for (let i = 0; i < 5; i++) {
      const fraction = i / 4;
      const waitMs = Math.round(fraction * duration);

      if (i === 0) {
        await new Promise(r => setTimeout(r, 500));
      } else {
        const prevFraction = (i - 1) / 4;
        await new Promise(r => setTimeout(r, Math.round((fraction - prevFraction) * duration)));
      }

      const frameFile = path.join(filmDir, `frame-${i}.png`);
      const tmpFile = path.join(filmDir, `frame-${i}.tmp.png`);
      await page.screenshot({ path: tmpFile });
      const image = sharp(tmpFile);
      const meta = await image.metadata();
      if (meta.width !== 720) {
        image.resize({ width: 720, fit: 'inside', withoutEnlargement: true });
      }
      await image.greyscale().toFile(frameFile);
      fs.unlinkSync(tmpFile);
      execSync(`convert "${frameFile}" -colorspace Gray "${frameFile}"`, { stdio: 'ignore' });
      frames.push({ file: path.relative(ROOT, frameFile), label: `frame-${i}`, time: waitMs });
      console.log(`    frame ${i}/5 @ ${waitMs}ms → frame-${i}.png`);
    }

    return { pass: true, animationDurationMs: duration, frames, keyframes: null, skipped: false };
  } finally {
    await browser.close();
  }
}

async function validateD3(specPath) {
  const results = { pass: [], fail: [], skipped: [] };
  const dirs = findArtifactDirs(specPath);

  let totalFound = 0;
  for (const [label, dir] of Object.entries(dirs)) {
    const htmlFiles = findFilesRecursive(dir, '.html');
    totalFound += htmlFiles.length;
    for (const htmlFile of htmlFiles) {
      const rel = path.relative(ROOT, htmlFile);
      console.log(`  Loading ${rel}...`);
      try {
        const filmResult = await captureFilmstrip(htmlFile);
        if (filmResult.pass) {
          results.pass.push({ file: rel, ...filmResult });
          console.log(`  ✓ ${rel} — ${filmResult.animationDurationMs}ms, ${filmResult.frames.length} frames`);
        } else {
          results.fail.push({ file: rel, error: filmResult.error });
          console.error(`  ✗ ${rel}: ${filmResult.error}`);
        }
      } catch (e) {
        results.fail.push({ file: rel, error: e.message });
        console.error(`  ✗ ${rel}: ${e.message}`);
      }
    }
  }

  if (totalFound === 0) {
    console.log('  (no HTML files found)');
  }

  return results;
}

function writeReports(mermaidResults, d3Results, specPath) {
  const dirs = findArtifactDirs(specPath);
  for (const [label, dir] of Object.entries(dirs)) {
    const report = {
      timestamp: new Date().toISOString(),
      mermaid: [],
      d3_filmstrip: []
    };

    const allResults = [
      ...mermaidResults.pass.map(r => ({ ...r, pass: true })),
      ...mermaidResults.fail.map(r => ({ ...r, pass: false })),
    ];
    for (const r of allResults) {
      const absFile = path.join(ROOT, r.file);
      if (absFile.startsWith(dir)) {
        report.mermaid.push(r);
      }
    }

    const allD3 = [
      ...d3Results.pass.map(r => ({ ...r, pass: true, skipped: false })),
      ...d3Results.fail.map(r => ({ ...r, pass: false, skipped: false })),
      ...d3Results.skipped.map(r => ({ ...r, pass: true, skipped: true })),
    ];
    for (const r of allD3) {
      const absFile = path.join(ROOT, r.file);
      if (absFile.startsWith(dir)) {
        report.d3_filmstrip.push(r);
      }
    }

    fs.writeFileSync(path.join(dir, 'report.json'), JSON.stringify(report, null, 2));
    console.log(`  report written to ${path.join(label, 'report.json')}`);
  }
}

async function main() {
  const args = process.argv.slice(2);
  const fileIdx = args.indexOf('--file');
  const specPath = fileIdx !== -1 ? args[fileIdx + 1] : null;

  if (fileIdx !== -1 && !specPath) {
    console.error('Usage: node scripts/test-artifacts.js [--file <relative-spec-path>]');
    process.exit(1);
  }
  if (specPath) {
    const absPath = path.resolve(ROOT, specPath);
    if (!fs.existsSync(absPath)) {
      console.error(`ERROR: Spec file not found: ${absPath}`);
      process.exit(1);
    }
  }

  console.log('=== Artifact Validation ===\n');

  console.log('Searching for artifact directories...');
  const dirs = findArtifactDirs(specPath);
  if (Object.keys(dirs).length === 0) {
    console.log('  No .artifacts/ directories found.');
    console.log('  Run `npm run extract -- --all` first to extract artifacts from spec files.\n');
    return;
  }
  for (const [label] of Object.entries(dirs)) {
    console.log(`  found ${label}/`);
  }

  console.log('\n--- Mermaid Diagrams ---');
  const mermaidResults = await validateMermaid(specPath);
  const mMetrics = {
    passed: mermaidResults.pass.length,
    failed: mermaidResults.fail.length,
    total: mermaidResults.pass.length + mermaidResults.fail.length,
  };
  console.log(`\n  ${mMetrics.passed} passed, ${mMetrics.failed} failed${mMetrics.total > 0 ? ` (${Math.round(mMetrics.passed / mMetrics.total * 100)}%)` : ''}\n`);

  console.log('--- D3 Animation Filmstrip ---');
  const d3Results = await validateD3(specPath);
  const dMetrics = {
    passed: d3Results.pass.length,
    failed: d3Results.fail.length,
    skipped: d3Results.skipped.length
  };
  console.log(`\n  ${dMetrics.passed} captured, ${dMetrics.failed} failed, ${dMetrics.skipped} skipped\n`);

  console.log('--- Writing Reports ---');
  writeReports(mermaidResults, d3Results, specPath);

  const totalFail = mermaidResults.fail.length + d3Results.fail.length;
  console.log(`\n=== Summary ===`);
  console.log(`  Mermaid: ${mMetrics.passed}/${mMetrics.total} passed`);
  console.log(`  D3 Filmstrip: ${dMetrics.passed} captured, ${dMetrics.failed} failed`);

  if (totalFail > 0) {
    console.error(`\n✗ ${totalFail} artifact(s) failed validation.`);
    process.exit(1);
  }

  console.log('\n✓ All artifact validations passed.');
}

main().catch(e => {
  console.error('Fatal error:', e);
  process.exit(1);
});
