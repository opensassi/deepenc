#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const SPEC_FILE = 'technical-specification.md';

function parseModuleReference() {
  const content = fs.readFileSync(path.join(ROOT, SPEC_FILE), 'utf-8');
  const lines = content.split('\n');

  let inTable = false;
  const rows = [];

  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed.startsWith('|') || !trimmed.endsWith('|')) {
      if (inTable) break;
      continue;
    }

    const cells = trimmed.split('|').map(c => c.trim()).filter(c => c.length > 0);

    if (!inTable) {
      inTable = true;
      continue;
    }

    if (cells.every(c => /^[-:` ]+$/.test(c))) continue;

    rows.push(cells);
  }

  const modules = {};
  for (const row of rows) {
    if (row.length < 4) continue;
    const name = row[0];
    const dir = row[1].replace(/`/g, '').trim();
    const specRaw = row[3];

    if (dir === '—' || dir === '-') continue;

    const specFiles = specRaw
      .split(',')
      .map(s => {
        const cleaned = s.replace(/`/g, '').trim();
        const parenIdx = cleaned.indexOf('(');
        return parenIdx !== -1 ? cleaned.substring(0, parenIdx).trim() : cleaned;
      })
      .filter(s => s.endsWith('.spec.md') && s.length > 0);

    modules[name.toLowerCase()] = { name, dir, specFiles };
  }

  return modules;
}

function extractCodeBlocks(content) {
  const blocks = [];
  const regex = /```(mermaid|html)\s*\n([\s\S]*?)```/g;
  let match;
  while ((match = regex.exec(content)) !== null) {
    const code = match[2].trim();
    if (code.length > 0) {
      blocks.push({ lang: match[1], code });
    }
  }
  return blocks;
}

function getDiagramName(code, lang, index) {
  if (lang === 'html') return 'd3-animation.html';
  const firstLine = code.trim().split('\n')[0].trim();
  if (/^graph\s+(TB|LR|RL|BT|TD)/.test(firstLine)) return 'architecture.mmd';
  if (/^sequenceDiagram/.test(firstLine)) return 'sequence.mmd';
  return `diagram-${index + 1}.mmd`;
}

function processSpecFile(specPath, outputDir, extraFiles) {
  const content = fs.readFileSync(specPath, 'utf-8');
  const blocks = extractCodeBlocks(content);

  const specName = path.basename(specPath);
  const specOutputDir = path.join(outputDir, specName);
  fs.mkdirSync(specOutputDir, { recursive: true });

  const usedNames = new Set();
  for (let i = 0; i < blocks.length; i++) {
    const { lang, code } = blocks[i];
    let filename = getDiagramName(code, lang, i);
    if (usedNames.has(filename)) {
      const base = filename.replace(/\.(mmd|html)$/, '');
      const ext = filename.match(/\.(mmd|html)$/)[0];
      let counter = 2;
      while (usedNames.has(`${base}-${counter}${ext}`)) counter++;
      filename = `${base}-${counter}${ext}`;
    }
    usedNames.add(filename);
    fs.writeFileSync(path.join(specOutputDir, filename), code + '\n');
    console.log(`  extracted ${specName}/${filename}`);
  }

  if (extraFiles) {
    for (const { src, dest } of extraFiles) {
      if (fs.existsSync(src)) {
        fs.copyFileSync(src, path.join(specOutputDir, dest));
        console.log(`  copied ${dest} from project root`);
      }
    }
  }

  const hasMermaid = blocks.some(b => b.lang === 'mermaid');
  if (blocks.length === 0 && !extraFiles?.some(e => fs.existsSync(e.src))) {
    console.error(`ERROR: No artifacts found in ${specPath}`);
    console.error(`HINT: Run system-design skill to regenerate spec with diagrams.`);
    process.exit(1);
  }
  if (!hasMermaid && blocks.length > 0) {
    console.log(`  (no mermaid diagrams in this spec file — HTML only)`);
  }
}

function processSubModule(mod) {
  const outputDir = path.join(ROOT, mod.dir, '.artifacts');
  fs.mkdirSync(outputDir, { recursive: true });
  console.log(`\nProcessing ${mod.name} module → ${path.join(mod.dir, '.artifacts/')}`);
  for (const specFile of mod.specFiles) {
    const specPath = path.join(ROOT, mod.dir, specFile);
    if (fs.existsSync(specPath)) {
      processSpecFile(specPath, outputDir);
    } else {
      console.error(`  WARNING: ${path.join(mod.dir, specFile)} not found, skipping`);
    }
  }
}

function main() {
  const args = process.argv.slice(2);
  const subModuleIdx = args.indexOf('--sub-module');
  const allFlag = args.includes('--all');
  const fileIdx = args.indexOf('--file');

  if (fileIdx !== -1) {
    const filePath = args[fileIdx + 1];
    if (!filePath) {
      console.error('Usage: node scripts/extract-artifacts.js --file <relative-path>');
      process.exit(1);
    }
    const absPath = path.resolve(ROOT, filePath);
    if (!fs.existsSync(absPath)) {
      console.error(`ERROR: File not found: ${absPath}`);
      process.exit(1);
    }
    const outputDir = path.join(path.dirname(absPath), '.artifacts');
    fs.mkdirSync(outputDir, { recursive: true });
    console.log(`Processing ${filePath} → ${path.join(path.dirname(filePath), '.artifacts/')}`);
    processSpecFile(absPath, outputDir);
    console.log('\nDone. Run `npm run test-artifacts` to validate.');
    return;
  }

  const modules = parseModuleReference();

  if (subModuleIdx !== -1) {
    const moduleName = args[subModuleIdx + 1];
    if (!moduleName) {
      console.error('Usage: node scripts/extract-artifacts.js --sub-module <name>');
      console.error(`Available modules: ${Object.keys(modules).join(', ')}`);
      process.exit(1);
    }
    const mod = modules[moduleName.toLowerCase()];
    if (!mod) {
      console.error(`Unknown sub-module: "${moduleName}". Available: ${Object.keys(modules).join(', ')}`);
      process.exit(1);
    }
    processSubModule(mod);
    return;
  }

  if (allFlag) {
    console.log(`Processing ${SPEC_FILE} → .artifacts/`);
    const rootD3 = path.join(ROOT, 'd3-animation.html');
    const rootExtra = fs.existsSync(rootD3) ? [{ src: rootD3, dest: 'd3-animation.html' }] : [];
    processSpecFile(path.join(ROOT, SPEC_FILE), path.join(ROOT, '.artifacts'), rootExtra);
    console.log('Extracting artifacts from all modules...');
    for (const mod of Object.values(modules)) {
      processSubModule(mod);
    }
    console.log('\nDone. Run `npm run test-artifacts` to validate.');
    return;
  }

  const outputDir = path.join(ROOT, '.artifacts');
  fs.mkdirSync(outputDir, { recursive: true });
  console.log(`Processing ${SPEC_FILE} → .artifacts/`);
  const rootD3 = path.join(ROOT, 'd3-animation.html');
  const extraFiles = fs.existsSync(rootD3) ? [{ src: rootD3, dest: 'd3-animation.html' }] : [];
  processSpecFile(path.join(ROOT, SPEC_FILE), outputDir, extraFiles);
  console.log('\nDone. Run `npm run test-artifacts` to validate.');
}

main();
