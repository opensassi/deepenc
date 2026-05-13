# deepenc

deepenc is an AI-driven optimization project for the [VVenC](https://github.com/fraunhoferhhi/vvenc) H.266/VVC encoder. It provides a harness for AI-assisted kernel optimization through CPU state tracing, an agent-based optimization framework, and a collaborative benchmark ecosystem.

deepenc, based on the Fraunhofer Versatile Video Encoder, is a fast and efficient software H.266/VVC encoder implementation with the following main features:

- Easy to use encoder implementation with five predefined quality/speed presets;
- Perceptual optimization to improve subjective video quality, based on the XPSNR visual model;
- Extensive frame-level and task-based parallelization with very good scaling;
- Frame-level single-pass and two-pass rate control supporting variable bit-rate (VBR) encoding.

## Prerequisites

The top-level script auto-detects your operating system and runs the correct installer:

```bash
bash scripts/install.sh
```

Platform-specific installers are also available directly:

| Platform | Command |
|----------|---------|
| Ubuntu 24.04 | `bash scripts/install/linux/ubuntu-noble-24.04/install.sh` |
| macOS 11+ | `bash scripts/install/osx/macos-sequoia-15.0/install.sh` |
| Windows (WSL2) | `powershell -ExecutionPolicy Bypass -File scripts\install.ps1` |

The Ubuntu installer provides the full toolchain: build-essential, cmake, nasm, git, nodejs/npm for the artifact pipeline, perf for profiling, gdb/gdb-mcp-server for debugging, and general-purpose CLI tools (ripgrep, fd, bat, htop, jq, httpie, tmux, etc.).

The macOS installer uses Homebrew and Xcode Command Line Tools. Note that `perf`, `strace`, and `ltrace` are Linux-specific; use Xcode Instruments.app for profiling and `lldb` for debugging.

The Windows installer sets up WSL2 with Ubuntu, provisions the repository inside WSL, and runs the Ubuntu install script within the WSL environment. A reboot may be required for the initial WSL feature installation.

## Information

See the [Wiki-Page](https://github.com/opensassi/deepenc/wiki) for more information:

- [Build information](https://github.com/opensassi/deepenc/wiki/Build)
- [Usage documentation](https://github.com/opensassi/deepenc/wiki/Usage)
- [Encoder performance](https://github.com/opensassi/deepenc/wiki/Encoder-Performance)
- [License](https://github.com/opensassi/deepenc/wiki/License)
- [Publications](https://github.com/opensassi/deepenc/wiki/Publications)
- [Version history](https://github.com/opensassi/deepenc/wiki/Changelog)

## Build

deepenc uses CMake to describe and manage the build process. A working [CMake](https://cmake.org/) installation is required to build the software. In the following, the basic build steps are described. Please refer to the [Wiki](https://github.com/opensassi/deepenc/wiki/Build) for the description of all build options.

### How to build using CMake?

To build using CMake, create a `build` directory and generate the project:

```sh
mkdir build
cd build
cmake .. <build options>
```

To actually build the project, run the following after completing project generation:

```sh
cmake --build .
```

For multi-configuration projects (e.g. Visual Studio or Xcode) specify `--config Release` to build the release configuration.

### How to build using GNU Make?

On top of the CMake build system, convenience Makefile is provided to simplify the build process. To build using GNU Make please run the following:

```sh
make install-release <options>
```

Other supported build targets include `configure`, `release`, `debug`, `relwithdebinfo`, `test`, and `clean`. Refer to the Wiki for a full list of supported features.

### Floating-point contraction and bit-exactness for AArch64

This improves performance but may result in output mismatches between platforms or compiler versions.
By default, deepenc allows the compiler to apply floating-point contraction (e.g. fusing multiply and add operations into fused multiply-add instructions).

To guarantee fully bit-exact output across builds, deepenc must be built with floating-point contraction disabled.

When configuring the build directly with CMake:

```sh
mkdir build
cd build
cmake .. -DVVENC_FFP_CONTRACT_OFF=On <other build options>
```

When using the provided Makefile wrapper:

```sh
make install-release ffp-contract-off=On <other options>
```

## Citing

Please use the following citation when referencing deepenc in literature:

```bibtex
@InProceedings{VVenC,
  author    = {Wieckowski, Adam and Brandenburg, Jens and Hinz, Tobias and Bartnik, Christian and George, Valeri and Hege, Gabriel and Helmrich, Christian and Henkel, Anastasia and Lehmann, Christian and Stoffers, Christian and Zupancic, Ivan and Bross, Benjamin and Marpe, Detlev},
  booktitle = {Proc. IEEE International Conference on Multimedia Expo Workshops (ICMEW)},
  date      = {2021},
  title     = {VVenC: An Open And Optimized VVC Encoder Implementation},
  doi       = {10.1109/ICMEW53276.2021.9455944},
  pages     = {1-2},
}
```

## Contributing

Feel free to contribute. To do so:

- Fork the current-most state of the master branch
- Apply the desired changes
- For non-trivial contributions, add your name to [AUTHORS.md](./AUTHORS.md)
- Create a pull-request to the upstream repository

## License

Please see [LICENSE.txt](./LICENSE.txt) file for the terms of use of the contents of this repository.

For more information, please contact: vvc@hhi.fraunhofer.de

**Copyright (c) 2019-2026, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. & The VVenC Authors.**

**All rights reserved.**

**VVenC® is a registered trademark of the Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V.**

## deepenc Approach

deepenc provides an open-source harness that transforms the VVenC H.266 encoder through an AI-driven, self-organizing competitive marketplace and a global video world-model corpus.

### Usage

You can run the harness directly without cloning the repository using `npx`.

```bash
npx vvenc-harness --help
```

#### Global Options

| Option              | Description                           |
| ------------------- | ------------------------------------- |
| `--config <path>`   | Path to configuration file            |
| `--verbose`         | Enable verbose logging                |
| `--output <format>` | Output format (`text`, `json`, `csv`) |

#### Commands

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
| `corpus query`        | Query the video world-model corpus                  |
| `corpus export`       | Export a training dataset from the corpus           |
| `instrument apply`    | Apply instrumentation patches to VVenC source       |
| `instrument verify`   | Verify instrumented encoder bitstream integrity     |
| `instrument revert`   | Remove instrumentation patches                      |

#### Example

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

# Verify a community-published result
npx vvenc-harness benchmark verify --result result.json
```

### Development Workflow

| Script             | Purpose                                                            |
| ------------------ | ------------------------------------------------------------------ |
| `npm install`      | Install all project dependencies (exact versions pinned)           |
| `npm run build`    | Compile TypeScript sources to `lib/` (using strict mode and ESM)   |
| `npm test`         | Run the full unit and integration test suite with Jest             |
| `npm run coverage` | Run tests **and** enforce 90% coverage thresholds; fail if not met |
| `npm run lint`     | Lint all TypeScript sources with ESLint and `@typescript-eslint`   |
| `npm run format`   | Auto-format code with Prettier                                     |
| `npm run prepare`  | Build the library (automatically triggered by `npm install`)       |

### Testing Guidelines

- Tests are co-located with the source files they test (e.g., `src/foo.test.ts`).
- Jest is configured with `ts-jest` for seamless ESM support.
- All tests run in a Node environment; no DOM.
- External systems (trace generator's CPU simulator, VTM decoder, VVdeC decoder, etc.) are mocked using lightweight, in-memory implementations.
- The end-to-end testing strategy uses mock servers and a Docker Compose stack where real-world integration is needed.
- **Coverage thresholds are strictly enforced at 90%** (branches, functions, lines, statements).
- Write tests that exercise all public methods and all major code paths; use the unit test table in the technical specification as a guide.

### AI Usage in Development

This project is developed with the assistance of AI tools.

- **Tools**: Visual Studio Code, Cline (and its fork Dirac), DeepSeek (via API and open-weights models).
- **Key files**:
  - `technical-specification.md` – the complete system design, diagrams, and testing plan.
- **Specification creation**: `technical-specification.md` was generated iteratively using an interactive system design agent prompt.
- **Development loop**:
  1. Refine the design with the design agent until the specification is stable.
  2. Hand the final specification to a coding agent (via Cline/Dirac) to generate the complete npm package, run the tests, and meet the coverage thresholds.
  3. If issues are found, iterate on the specification before re-generating the code.
