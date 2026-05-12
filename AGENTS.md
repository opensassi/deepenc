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

# GDB Debugger MCP — Headless Debugging for C++

The GDB debugger MCP is provided by `gdb-mcp-server` (Ipiano/gdb-mcp), a Python-based MCP server using
the GDB/MI protocol. Unlike GDB-only tools with `OsString` serialization bugs, this server accepts
`args` as a plain `list[str]` and supports `env` dicts and `init_commands`.

## Config

The debugger is configured in `.opencode/opencode.json`:

```json
"debugger": {
  "type": "local",
  "command": ["gdb-mcp-server"],
  "enabled": true
}
```

## Available tools

| Tool | Purpose |
|---|---|
| `gdb_start_session` | Start a session with `program`, `args` (list), `env` (dict), `core`, `init_commands` |
| `gdb_execute_command` | Run any GDB command (CLI or MI) |
| `gdb_get_backtrace` | Stack trace for a thread |
| `gdb_get_variables` | Local variables for a frame |
| `gdb_get_registers` / `gdb_get_threads` | Register / thread inspection |
| `gdb_set_breakpoint` / `gdb_list_breakpoints` / `gdb_delete_breakpoint` | Breakpoint management |
| `gdb_continue` / `gdb_step` / `gdb_next` / `gdb_interrupt` | Execution control |
| `gdb_evaluate_expression` | Evaluate a C/C++ expression |
| `gdb_call_function` | Call a function in the target process |
| `gdb_get_status` / `gdb_stop_session` | Session lifecycle |

## Usage in prompts

Use `gdb_start_session` with the program, args, and env:

```
gdb_start_session
  program: ./bin/debug-static/vvencapp
  args: ["-i", "test/data/park_joy.yuv", "-s", "832x480", "-f", "50", "--preset", "fast", "--qp", "22", "--frames", "2", "-o", "/dev/null"]
  env: {"VVENC_TRAINING_OUT": "/tmp/train.csv"}
```

## When to use the debugger

### 1. Config propagation failure

When a value set in `vvencimpl.cpp::init()` is not visible downstream:

- Set breakpoints at the setting site and the consumption site
- Check struct field values with `gdb_evaluate_expression`
- Example: `m_trainingOutputFile` set after `initEncoderLib()` doesn't propagate

### 2. Segfault / uninitialized memory

When the encoder crashes inside a C++ function:

```
gdb_start_session program: ./bin/debug-static/vvencapp args: [...]
gdb_set_breakpoint location: "EncCu.cpp:996"
gdb_continue
gdb_get_backtrace
```

- Garbage pointers look like `0x3ff15c28f5c28f5c` (floating-point NaN bit pattern)
- Null pointers are `0x0`
- Get the full backtrace with `gdb_get_backtrace`
- Inspect locals with `gdb_get_variables`

### 3. CU lifecycle / data flow

When a data structure has unexpected state:

- Set breakpoints at multiple points in the same function
- Use `gdb_evaluate_expression` to inspect pointer chains
- Compare values between pre-split and post-split execution points

### 4. Thread safety

When output data is corrupted from concurrent writes:

- Check if static/global variables are accessed from multiple threads
- Use `gdb_get_threads` to list active threads
- Set breakpoints and check which thread hits them

## Session lifecycle

1. **Start** → `gdb_start_session` (loads program, sets env)
2. **Run** → `gdb_continue` or `gdb_execute_command` with `"run"`
3. **Inspect** → `gdb_get_backtrace`, `gdb_get_variables`, `gdb_evaluate_expression`
4. **Stop** → `gdb_stop_session`

If the program is running and not hitting a breakpoint, use `gdb_interrupt` to pause it.

## Build prerequisites

Debug builds require `-DCMAKE_BUILD_TYPE=Debug`:

```bash
cmake -B build_debug -S . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVVENC_ENABLE_ML_LIGHTGBM=ON \
  -DVVENC_ENABLE_AI_TRAINING=ON \
  -DLightGBM_LIBRARY=/usr/local/lib/lib_lightgbm.so \
  -DLightGBM_INCLUDE_DIR=/usr/local/include
cmake --build build_debug -j$(nproc) --target vvencapp
```

# Dev Environment Setup

## GitHub CLI (for the `issue` skill)

```bash
sudo apt install gh
gh auth login
```

Follow the browser-based OAuth flow or paste a personal access token. Verify with:

```bash
gh issue list --repo opensassi/deepenc
```

## LightGBM ML Module (optional, for ML-guided CU partitioning)

Add the Microsoft package repository and install:

```bash
wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor | \
  sudo tee /usr/share/keyrings/microsoft.gpg > /dev/null
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/microsoft.gpg] \
  https://packages.microsoft.com/ubuntu/24.04/prod noble main" | \
  sudo tee /etc/apt/sources.list.d/microsoft.list
sudo apt update && sudo apt install liblightgbm-dev
```

Build with:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DVVENC_ENABLE_ML_LIGHTGBM=ON
```
