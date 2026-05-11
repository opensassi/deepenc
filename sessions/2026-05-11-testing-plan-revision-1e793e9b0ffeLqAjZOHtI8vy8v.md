**Session ID:** 2026-05-11-testing-plan-revision

**Date / Duration:** May 11, 2026; prompter active ≈ 0.8 hours

**Project / Context:**
Revision of the `system-design` opencode skill's testing plan definitions and addition of a frozen regression test baseline to the project's `technical-specification.md` for the **deepenc** project — a C++14 fork of Fraunhofer VVenC (H.266/VVC encoder). The session also included a persona update from the prior session's crypto architect change.

**Top-Level Component:**
`technical-specification.md § C++ Coding Conventions > Regression Test Baseline` — a hard-coded table listing all 4 frozen test file groups (C API interface test, C++ SDK functional tests with 6 CTest variants, unit tests, integration tests) that must never be modified. Establishes the rule that new tests go in new files only.

**Second-Level Modules:**
- `system-design` SKILL.md persona revision (crypto → video encoding, applied from prior design)
- `system-design` SKILL.md `generate testing plan` command — rewritten to describe the project's actual three-tier CTest structure, reference the regression baseline, and mandate new-file-only additions
- `system-design` SKILL.md `generate technical specification` testing requirements — replaced mock-server/streaming/SME language with calling-order validation, parameter range tests, custom TEST macros, real encoder instances, bit-exact output comparison, and CTest fixture cleanup
- Design principle: "do not modify baseline files" instruction embedded in both skill sections

**Prompter Contributions:**
- Identified that the existing testing plan language (mock servers, streaming/SSE, session management) did not match the project
- Clarified the key requirement: baseline test files must be immutable and never modified
- Directed that the hard-coded list live in `technical-specification.md` as the single source of truth, with the skill cross-referencing it
- Approved the final proposal and issued execution commands

**Model Contributions:**
- Analyzed the actual project test structure by reading `vvencTests.cmake`, all 3 test `CMakeLists.txt` files, and the `vvencTests.cmake` registration logic
- Identified the specific mismatch between the skill's generic testing plan and the project's actual three-tier structure
- Drafted the 15-line regression baseline table with exact CTest registration names, file paths, types, and scope
- Rewrote both `generate testing plan` and `generate technical specification` testing requirements to match the project's patterns
- Added the immutable-regression rule to the `generate technical specification` shared constraints

**Prompter Time Estimate:**
- Reading and digesting model responses: ~0.3 hours
- Thinking, strategizing, and weighing options: ~0.3 hours
- Writing messages and directives: ~0.2 hours
- **Total: 0.8 hours** (single sitting)

**Model-Equivalent SME Time Estimate:**
~4 hours — A senior build/test engineer would need approximately:
- 1h to read and understand the existing CTest infrastructure and test file structure
- 1h to identify which files are the regression baseline and document them
- 1.5h to revise the generate testing plan and testing requirements sections
- 0.5h to cross-reference and verify consistency between `technical-specification.md` and the skill

**Required SME Expertise:**
- CTest/CMake test infrastructure design for C/C++ video codec projects
- Video encoder testing methodology (calling-order, parameter ranges, bit-exact output comparison, multi-pass rate control)
- Custom test macro frameworks (TEST/TESTT/ERROR with global counters)
- opencode skill development and cross-document reference management
- Technical specification writing for frozen regression baseline policies

**Aggregation Tags:**
testing-plan, regression-baseline, CTest, VVenC, deepenc, system-design, skill-management, test-infrastructure, C++ testing, encoder validation
