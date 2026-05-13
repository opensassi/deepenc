**Session ID:** 2026-05-12-skill-manager-revisions

**Date / Duration:** 2026-05-12; prompter active ≈ 1.5 hours

**Project / Context:**
Deepenc opencode skill management — revising existing skills (skill-manager, git) for better workflow enforcement, and creating a new `todo` skill that formalizes the "create GitHub issue + linked debugging skill from session context" pattern demonstrated in the preceding asm-optimizer session.

**Top-Level Component:**
Three skill revisions and one new skill: skill-manager (unregistered skill detection, registration verification), git (export workflow fix, artifact validation), and todo (extract → issue → skill pipeline).

**Second-Level Modules:**
- skill-manager: added "Detect unregistered skills" to Response Guidelines, added "No unregistered skills" design principle, beefed up `save skill` step 3 with post-write verification, added `audit skills` command
- git: fixed `finish session` step 10-12 to use `bash sessions/export-session.sh` instead of broken `opencode session export`, added artifact validation (non-zero check), updated ordering constraint note
- opencode.json: registered `had-avx2-optimization-4` skill (previously orphaned on disk)
- `todo` skill: new skill for extracting session context, creating issues via the `issue` skill, and producing linked debugging skills (pattern: extract → propose-issue → create-issue → propose-skill → save-skill)

**Prompter Contributions:**
- Directed the `audit skills` command design
- Requested "Detect unregistered skills" as an activation-time check
- Requested export workflow fix in git skill based on observed failure
- Named the new skill `todo` and chose option A (depend on `issue` skill)

**Model Contributions:**
- Performed skill audit (read all existing skills, identified gaps)
- Drafted and applied both git skill fixes (export command + validation)
- Drafted and applied skill-manager revisions (unregistered detection, registration verification, audit command)
- Proposed, iterated, and saved the `todo` skill
- Registered `had-avx2-optimization-4` in opencode.json

**Prompter Time Estimate:**
- Reading and digesting model responses: ~0.5 hours
- Thinking, strategizing, and weighing options: ~0.5 hours
- Writing messages and directives: ~0.5 hours
- **Total: 1.5 hours**

**Model-Equivalent SME Time Estimate:**
~6 hours of senior tooling architect time:
- Skill audit and gap analysis: 1.5 hours
- git skill revisions (export fix + validation): 1.5 hours
- skill-manager revisions (audit command, registration checks): 1.5 hours
- todo skill design and creation: 1.5 hours

**Required SME Expertise:**
- opencode skill architecture and SKILL.md conventions
- Git rebase workflow design and CI pipeline management
- Session evaluation export pipeline debugging
- Technical writing for LLM agent instructions
- Interactive propose-revise-save workflow design
- GitHub issue template design for agent consumption

**Aggregation Tags:**
skill-management, opencode, git-workflow, session-evaluation, issue-creation, audit, todo, debugging-skills, workflow-automation
