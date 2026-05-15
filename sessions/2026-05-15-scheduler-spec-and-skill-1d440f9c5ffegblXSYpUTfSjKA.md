**Session ID:** 2026-05-15-scheduler-spec-and-skill

**Date / Duration:** 2026-05-15; prompter active ≈ 4.5 hours

**Project / Context:**
Design and specification of a decomposed TU pipeline scheduler for the VVenC H.266/VVC encoder (deepenc fork). The work includes architecting a fine-grained work-unit dispatch system, wavefront CTU parallelism layer, and standalone microbenchmark harness — all specified as C++14-class-level spec files with Mermaid and D3 visualization artifacts.

**Top-Level Component:**
Scheduler module technical specification (7 .spec.md files + 1 tool spec) with 14 validated artifacts (10 Mermaid diagrams, 4 D3 animations).

**Second-Level Modules:**
- Aggregate Scheduler module spec with architecture diagram, sequence diagram, and D3 animation of batch dispatch
- TUScheduler facade class spec with pool state machine visualization
- TUPipelineDAG intra-TU dependency graph builder spec with animated topological sort
- PictureDAG wavefront CTU dispatch spec with anti-diagonal grid animation
- RingBuffer lock-free intermediate storage spec
- WorkUnit struct and Stage enum spec (TU-level + CTU-level stages, spatial dependency types)
- SchedulerTrace binary capture format spec
- SchedulerBench replay harness CLI spec
- GitHub issue #9 created for Phase 1 implementation
- reusable debugging/project-management skill

**Prompter Contributions:**
- Defined the overall architecture vision: fine-grained work-unit decomposition with flexible batching
- Identified the key architectural tension (mode decision coupling) and steered toward Option A (serial mode trials)
- Specified the single-buffer-reuse scheme to address cache thrashing
- Clarified that bit-exactness is not a concern since operation sequences remain unchanged
- Directed the addition of the PictureDAG wavefront layer for frame-level parallelism
- Specified the compile-flag gating strategy (ENABLE_SCHEDULER_DISPATCH, ENABLE_SCHEDULER_TRACE)
- Defined the trace-capture + microbench replay architecture (root inputs only, no intermediate data)
- Chose the code organization: Scheduler/ as a standalone module directory with separate SchedulerBench/ tool
- Requested and approved the full sub-module spec file creation with all artifacts
- Requested the wavefront parallelism design review and subsequent integration
- Directed the todo/issue/skill workflow for Phase 1 planning

**Model Contributions:**
- Analyzed the existing VVenC codebase (TrQuant, DepQuant, mode decision, thread pool, buffer ownership) to inform design decisions
- Quantified memory tradeoffs of decomposition vs. cache residency
- Designed the WorkUnit struct, Stage enum, and dependency-graph architecture
- Wrote all 7 + 1 spec files totaling ~2,400 lines of spec content
- Created 10 Mermaid architecture and sequence diagrams
- Created 4 D3 animation HTML files totaling ~800 lines of animated visualization
- Updated technical-specification.md Module Reference (80→86 files)
- Performed extraction and validation of all 14 artifacts (confirmed all pass)
- Created GitHub issue #9 with structured scope, context, implementation notes, and acceptance criteria
- Drafted and saved the scheduler-phase-1-9 debugging skill to .opencode/skills/
- Registered skill in opencode.json

**Prompter Time Estimate:**
- Reading and digesting model responses (~50,000 words at 250 wpm + 20%): ~4.0 hours
- Thinking, strategizing, and weighing options: ~2.0 hours
- Writing messages and directives (~1,500 words at 120 wpm): ~0.3 hours
- **Total: ~6.3 hours** (over several sessions)

**Model-Equivalent SME Time Estimate:**
~120 hours (3 weeks). Breakdown:
- VVenC codebase exploration and analysis: 20 hours
- Scheduler architecture design and iteration: 16 hours
- C++ class specification writing (7 classes): 24 hours
- Mermaid diagram authoring (10 diagrams): 8 hours
- D3 animation programming (4 animations): 32 hours
- Validation and debugging of artifacts: 8 hours
- Issue and skill creation workflow: 12 hours

**Required SME Expertise:**
- VVC/H.266 encoder architecture (CU/TU partitioning, transform/quant pipeline, mode decision)
- C++14 systems programming with atomic concurrency primitives
- Lock-free data structure design (CAS-based slot pools)
- DAG-based scheduler architecture for parallel video encoding
- Wavefront parallelism (x265 WPP style anti-diagonal dispatch)
- Mermaid diagram authoring (C4 containers, sequence diagrams)
- D3.js animation programming (data joins, transitions, keyframe-based verification)
- GPU offload architecture awareness (batch-submit patterns, async completion)
- Video codec bit-exactness constraints and FP determinism
- CMake build system engineering (conditional compilation, multi-target)

**Aggregation Tags:**
scheduler, work-unit, DAG, wavefront, parallelism, VVenC, VVC, encoder, spec, system-design, architecture, C++14, D3, Mermaid
