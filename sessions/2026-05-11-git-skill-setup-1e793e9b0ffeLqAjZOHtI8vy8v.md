**Session ID:** 2026-05-11-git-skill-setup

**Date / Duration:** May 11, 2026; prompter active ≈ 0.5 hours

**Project / Context:**
Creation of the `git` opencode skill for the deepenc project — a rebase-based git workflow skill with integrated session evaluation, single atomic commits on main, and auto-push. Also updated `session-evaluation` to remove git operations (now handled by the `git` skill) and added a git workflow directive to `AGENTS.md`.

**Top-Level Component:**
`.opencode/skills/git/SKILL.md` — an 88-line skill defining `start session`, `finish session`, and `sync` commands with a full 13-step end-of-session pipeline (stage → generate → commit → rebase → test → amend → export → push).

**Second-Level Modules:**
- `session-evaluation` SKILL.md — removed `export` step 6 (git commit) and updated design principles to delegate git to the `git` skill
- `AGENTS.md` — appended git workflow directive instructing all agents to load the `git` skill before development
- `opencode.json` — registered `"git": "allow"` in skills permissions

**Prompter Contributions:**
- Specified the rebase-only, single-commit-on-main approach
- Specified the commit message format matching session evaluation filenames
- Directed that `session-evaluation` should not handle git operations
- Executed the final `finish session` command to validate the workflow end-to-end

**Model Contributions:**
- Researched opencode's skill, command, agent, and rules mechanisms to determine the right architecture
- Designed the 13-step `finish session` workflow with conflict handling, test failure loops, and amend-based artifact inclusion
- Drafted all three files (git skill, session-evaluation edits, AGENTS.md update)
- Applied all changes and registered the skill

**Prompter Time Estimate:**
- Reading and digesting model responses: ~0.2 hours
- Thinking, strategizing, and weighing options: ~0.2 hours
- Writing messages and directives: ~0.1 hours
- **Total: 0.5 hours**

**Model-Equivalent SME Time Estimate:**
~3 hours — A senior DevOps/ tooling engineer would need approximately:
- 1h to research opencode skill/command/agent capabilities
- 1h to design the git workflow and session evaluation integration
- 1h to implement, test, and register all artifacts

**Required SME Expertise:**
- opencode skill development and cross-skill orchestration
- Git rebase workflow design and conflict resolution strategies
- CTest/CMake test pipeline integration
- Session evaluation and archiving pipeline design

**Aggregation Tags:**
git-workflow, session-evaluation, AGENTS.md, opencode-skills, rebase-workflow, devops, CI-integration, deepenc
