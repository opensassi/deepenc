# VVenC AI‑Driven Optimization Ecosystem with Emergent Competitive Marketplace

An open‑source harness that transforms the VVenC H.266 encoder into the fastest software encoder through an AI‑driven, self‑organizing competitive marketplace and a global video world‑model corpus.

## Usage

You can run the harness directly without cloning the repository using `npx`.

```bash
npx vvenc-harness --help
```

### Global Options

| Option              | Description                           |
| ------------------- | ------------------------------------- |
| `--config <path>`   | Path to configuration file            |
| `--verbose`         | Enable verbose logging                |
| `--output <format>` | Output format (`text`, `json`, `csv`) |

### Commands

| Command               | Description                                         |
| --------------------- | --------------------------------------------------- |
| `trace generate`      | Generate CPU state traces for a hot function        |
| `trace validate`      | Validate an existing trace for internal consistency |
| `pyramid test`        | Run a specified tier against a candidate kernel     |
| `pyramid full`        | Run all tiers against a candidate kernel            |
| `agent run`           | Execute one optimization iteration                  |
| `agent session`       | Run a full optimization session                     |
| `benchmark export`    | Export a signed BenchmarkResult from a session      |
| `benchmark verify`    | Reproduce a benchmark result from a published file  |
| `metadata extract`    | Extract metadata from an encoding session           |
| `metadata anonymize`  | Anonymize extracted metadata                        |
| `metadata contribute` | Contribute anonymized metadata to the corpus        |
| `corpus query`        | Query the video world‑model corpus                  |
| `corpus export`       | Export a training dataset from the corpus           |
| `instrument apply`    | Apply instrumentation patches to VVenC source       |
| `instrument verify`   | Verify instrumented encoder bitstream integrity     |
| `instrument revert`   | Remove instrumentation patches                      |

### Example

```bash
# Run a full optimization session for the sad_16x16 kernel targeting Zen 4
npx vvenc-harness agent session \
  --function sad_16x16 \
  --arch znver4 \
  --llm my-model-v3

# Export the signed benchmark result
npx vvenc-harness benchmark export \
  --session-id abc123 \
  --key ./lab-key.pem > result.json

# Verify a community‑published result
npx vvenc-harness benchmark verify --result result.json
```

## Development

Clone the repository and install dependencies:

```bash
git clone <repository-url>
cd vvenc-harness
npm install
npm run build
```

## Development Workflow

| Script             | Purpose                                                            |
| ------------------ | ------------------------------------------------------------------ |
| `npm install`      | Install all project dependencies (exact versions pinned)           |
| `npm run build`    | Compile TypeScript sources to `lib/` (using strict mode and ESM)   |
| `npm test`         | Run the full unit and integration test suite with Jest             |
| `npm run coverage` | Run tests **and** enforce 90% coverage thresholds; fail if not met |
| `npm run lint`     | Lint all TypeScript sources with ESLint and `@typescript-eslint`   |
| `npm run format`   | Auto‑format code with Prettier                                     |
| `npm run prepare`  | Build the library (automatically triggered by `npm install`)       |

## Testing Guidelines

- Tests are co‑located with the source files they test (e.g., `src/foo.test.ts`).
- Jest is configured with `ts-jest` for seamless ESM support.
- All tests run in a Node environment; no DOM.
- External systems (`TraceGenerator`’s CPU simulator, VTM decoder, VVdeC decoder, etc.) are mocked using lightweight, in‑memory implementations.
- The end‑to‑end testing strategy uses mock servers and a Docker Compose stack where real‑world integration is needed.
- **Coverage thresholds are strictly enforced at 90%** (branches, functions, lines, statements). If coverage falls below the threshold, the `npm run coverage` script will exit with code 1, and the change must be reworked.
- Write tests that exercise all public methods and all major code paths; use the unit test table in the technical specification as a guide.

## AI Usage in Development

This project is developed with the assistance of AI tools.

- **Tools**: Visual Studio Code, Cline (and its fork Dirac), DeepSeek (via API and open‑weights models).
- **Key files**:
  - `technical-specification.md` – the complete system design, diagrams, and testing plan.
- **Specification creation**: `technical-specification.md` was generated iteratively using an “Interactive System Design Agent” prompt. This prompt enables a conversational design loop that produced the full specification, architecture diagram, sequence diagrams, and API contracts.
- **Development loop**:
  1. Refine the design with the design agent until the specification is stable.
  2. Hand the final specification to a coding agent (via Cline/Dirac) to generate the complete npm package, run the tests, and meet the coverage thresholds.
  3. If issues are found, iterate on the specification before re‑generating the code.
- **Model hosting**: For faster inference or data residency, open‑weights DeepSeek models can be served via US‑based endpoints such as NVIDIA NIM or HuggingFace Inference Endpoints. The coding agent is agnostic to the backend.

## Contributing

Contributions must adhere to the technical specification. Before opening a pull request:

- Ensure your code follows the project’s linting and formatting setup (`npm run lint` and `npm run format`).
- All existing and new tests must pass.
- The coverage threshold (90%) must be met; run `npm run coverage` to confirm.
- Do not modify the `coverageThreshold` values in `jest.config.ts`.
- If the specification is updated, regenerate the relevant parts of the implementation using the above AI‑assisted workflow.
