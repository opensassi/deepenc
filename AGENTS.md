# Playwright MCP — Headless Browser for Testing

Playwright MCP is configured in `.opencode/opencode.json`. It provides headless browser automation via accessibility snapshots (no vision model needed).

## Available tools

- `browser_navigate` — navigate to a URL
- `browser_snapshot` — capture accessibility tree of the page
- `browser_click` / `browser_type` / `browser_hover` — interact with elements
- `browser_evaluate` — run JS in page context
- `browser_take_screenshot` — capture screenshots
- `browser_console_messages` — read console output
- `browser_network_requests` / `browser_network_request` — inspect network
- `browser_fill_form` — fill multiple form fields at once
- `browser_tabs` — manage tabs
- `browser_wait_for` — wait for text or time

## Usage in prompts

Add `use the playwright tool` or reference specific tools like `use playwright browser_navigate` in prompts.

## Testing Graphasaurus

- The D3 animation (`d3-animation.html`) can be opened and verified in the headless browser
- HTTP shard endpoints can be navigated and their responses inspected
- Use `browser_console_messages` to check for JS errors after page loads

# Artifact Validation Pipeline

Mermaid diagrams and D3 animations embedded in spec files are extracted and validated automatically.

## Commands

```
npm run extract                    # extract artifacts from technical-specification.md → .artifacts/
npm run extract -- --sub-module core  # extract artifacts from src/core/*.spec.md → src/core/.artifacts/
npm run extract -- --all           # extract all artifacts (root + all modules)
npm run test-artifacts             # validate all extracted artifacts (mermaid → PNG, D3 → filmstrip)
npm run validate-all               # extract --all + test-artifacts in one step
```

## Directory structure

```
.artifacts/                              ← technical-specification.md artifacts
├── architecture.mmd / .png
├── sequence.mmd / .png
├── d3-animation.html
└── d3-animation-filmstrip/
    ├── frame-{number}-{keyframe-label}.png  (one per keyframe)

src/<module>/.artifacts/                 ← per-module spec file artifacts
├── <SpecFile>.spec.md/
│   ├── architecture.mmd / .png
│   └── sequence.mmd / .png
└── report.json
```

## Guard behavior

If a spec file contains no mermaid/html artifacts, `extract-artifacts.js` exits with an error:
`ERROR: No artifacts found in <file>. Run system-design skill to regenerate.`

The D3 filmstrip requires `window.ANIMATION_KEYFRAMES` (array of `{time, label}`) to be set in `d3-animation.html`. Until the full animation is generated via the system design workflow, this test will report: `ANIMATION_KEYFRAMES not set`.

# Git & Session Workflow

CRITICAL: Before any development work, load the `git` skill via the skill tool and follow its commands. Always develop directly against `main` with a rebase workflow. At the end of every session, run `finish session` which orchestrates commit → rebase → tests → evaluation → push.
