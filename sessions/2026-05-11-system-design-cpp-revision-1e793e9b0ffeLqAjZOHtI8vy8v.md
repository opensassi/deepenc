**Session ID:** 2026-05-11-system-design-cpp-revision

**Date / Duration:** May 11, 2026; prompter active ≈ 1.5 hours

**Project / Context:**
Revision of the `system-design` opencode skill for the **deepenc** project — a C++14 fork of Fraunhofer VVenC (H.266/VVC encoder). The session migrated the skill's specification language from TypeScript to C++, added a project-specific C++ coding conventions document rooted in actual codebase analysis, and updated the persona to reflect video encoding and SIMD expertise.

**Top-Level Component:**
`technical-specification.md § C++ Coding Conventions` — a 154-line reference documenting the project's C++14 idioms, naming conventions (`m_`/`x`/`m_p`/`m_b` prefixes), class patterns, error signaling, threading, callback, test, and preprocessor conventions — derived from analysis of 11 primary source files across the C API, C++ SDK, encoder library, and test suites.

**Second-Level Modules:**
- `system-design` SKILL.md persona rewrite (crypto architect → video encoding systems engineer)
- `generate class specification` command rewrite from TypeScript to C++ class declarations with project-specific checklist
- `generate technical specification` command — all TypeScript/JSDoc references replaced with C++/Doxygen equivalents
- Activation flow — new step 2 loads C++ conventions from `technical-specification.md` on startup
- Design principles — portability targets updated, naming conventions bullet added
- `generate sub-module spec` — facade description cross-references C++ conventions section

**Prompter Contributions:**
- Identified the need to switch from TypeScript to C++ as the primary specification language
- Directed the exploratory plan (build + test run, source analysis, then document)
- Chose to put project-specific instructions in `technical-specification.md` (not in the skill) and have the skill reference it
- Requested the persona change from cryptographic to video-encoding focus
- Issued explicit `save skill` commands at each persistence point

**Model Contributions:**
- Ran the full build+test pipeline (38/38 tests passed) via the deepenc-harness CLI
- Analyzed 11 primary C++ files for idiomatic patterns (naming, class structure, error handling, threading, test macros, preprocessor conventions)
- Drafted the 154-line C++ Coding Conventions section with 12 subsections, code examples, and reference tables
- Proposed and applied 6 structured revisions to `system-design` SKILL.md
- Proposed and applied the persona revision (7th change)
- Maintained cross-references between `technical-specification.md` and the skill across 4 distinct locations

**Prompter Time Estimate:**
- Reading and digesting model responses: ~0.6 hours
- Thinking, strategizing, and weighing options: ~0.5 hours
- Writing messages and directives: ~0.4 hours
- **Total: 1.5 hours** (single sitting)

**Model-Equivalent SME Time Estimate:**
~6 hours — A senior C++ tooling engineer would need approximately:
- 1h to read and understand the VVenC codebase structure and conventions
- 1h to run tests and verify build pipeline
- 2h to distill naming conventions, class patterns, and idioms from 11+ files
- 1h to author the C++ conventions document
- 1h to revise the system-design skill with all cross-references

**Required SME Expertise:**
- C++14/17 project structure and CMake build system design
- H.266/VVC video encoder architecture and SIMD optimization (SSE4.1, ARM Neon/SVE)
- Video codec testing methodology (CTest, custom test macros, manual runners)
- opencode skill development and SKILL.md prompt engineering
- Technical specification writing for performance-critical C++ systems
- Cross-referencing and document consistency management

**Aggregation Tags:**
system-design, skill-management, C++ conventions, VVenC, deepenc, video encoding, SIMD, H.266, VVC, technical-specification, CMake, codebase analysis
