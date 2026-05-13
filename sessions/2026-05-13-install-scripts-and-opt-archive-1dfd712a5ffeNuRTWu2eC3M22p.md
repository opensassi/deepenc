**Session ID:** 2026-05-13-install-scripts-and-opt-archive

**Date / Duration:** 2026-05-13; prompter active ≈ 3.5 hours

**Project / Context:**
deepenc is an AI-driven optimization project for the VVenC H.266/VVC encoder. This session covered the tail end of an ASM optimization attempt (interpolation filters), cleanup/archiving of unsuccessful work, creation of cross-platform install scripts, and project documentation updates.

**Top-Level Component:**
Platform install scripts for Ubuntu, macOS, and Windows (WSL2), plus experiment archiving for the interp-filterHor/vert optimization attempt.

**Second-Level Modules:**
- Experiment archive for interp-filterHor/vert optimization work (README, src, specs, results)
- Cleanup: removed stale ASM files, restored asm-primitives stubs, preserved NASM build infra
- Install script: Ubuntu noble (`scripts/install/linux/ubuntu-noble-24.04/install.sh`)
- Install script: macOS sequoia (`scripts/install/osx/macos-sequoia-15.0/install.sh`)
- Install script: Windows WSL2 (`scripts/install/windows/wsl2/install.ps1`)
- Top-level dispatcher (`scripts/install.sh` for Linux/macOS, `scripts/install.ps1` for Windows)
- README.md rewrite with prerequisites section and platform-specific install instructions
- AGENTS.md update with full CLI tools reference table
- Project documentation for vertical filter debugging issue (GitHub issue #5, skill `interp-vert-8tap-debug-5`)

**Prompter Contributions:**
- Directed the scope and priorities of the optimization attempt (filterHor → filterVer → 2D)
- Recognized when the vertical filter debugging was stalling and redirected to archival
- Made architecture decisions for install script structure (osx/windows separation, NAME-VERSION_CODENAME naming, directory conventions)
- Provided detailed requirements for the finish session workflow and evaluation format
- Decided on the comprehensive "install everything" approach with full CLI tooling
- Corrected directory naming conventions through multiple iterations

**Model Contributions:**
- Researched and verified all 38 Ubuntu package names against the Noble repository
- Built macOS package equivalent table with Homebrew lookups
- Created three platform-specific install scripts with version detection and codename mapping
- Wrote the top-level shell/PowerShell dispatchers with OS detection logic
- Generated the session evaluation using the structured template
- Analyzed NASM VEX encoding issues and implemented bit-exact horizontal filter
- Produced debug traces (GDB register dumps) for vertical filter analysis
- Created experiment archive with README, benchmark results, and spec artifacts

**Prompter Time Estimate:**
- Reading and digesting model responses: ~1.5 hours
- Thinking, strategizing, and weighing options: ~1.0 hours
- Writing messages and directives: ~1.0 hours
- **Total: 3.5 hours**

**Model-Equivalent SME Time Estimate:**
~32 hours — including:
- ASM optimization research and implementation: 12 hours
- Debugging VEX encoding issues across GAS and NASM: 4 hours
- Build system integration (CMake NASM support): 3 hours
- Cross-platform install scripting (3 platforms, package research, testing): 8 hours
- Documentation and archival: 3 hours
- Session evaluation and project management: 2 hours

**Required SME Expertise:**
- x86-64 AVX2 assembly programming with NASM/GAS syntax
- VEX instruction encoding (2-byte vs 3-byte prefix behavior)
- Video codec DCT-IF interpolation filter implementation (horizontal and vertical)
- CMake build system engineering with cross-language compilation (C++ + NASM)
- Debugging with GDB, objdump, perf stat, and register-value analysis
- Cross-platform development environment setup (Ubuntu, macOS, Windows/WSL2)
- Shell scripting (bash) and PowerShell scripting
- Homebrew package management on macOS
- Technical writing and project documentation

**Aggregation Tags:**
AVX2, NASM, VEX, interpolation-filter, x86-optimization, install-scripts, ubuntu, macos, windows, wsl2, build-infrastructure, session-evaluation
