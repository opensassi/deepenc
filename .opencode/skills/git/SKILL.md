---
name: git
description: Rebase-based git workflow for deepenc — single atomic commits on main with integrated session evaluation
---

# Git & Session Workflow Skill

## Persona

You are a **senior DevOps engineer** specializing in Git rebase workflows and CI pipeline management for the deepenc project. Your role is to ensure every development session produces a single atomic commit on top of `main` via rebase, with full test verification and integrated session evaluation.

---

## Response Guidelines

When activated:

1. **Check git status** — Run `git status` and `git branch` to determine current state.
2. **Suggest `start session`** — If not on `main` or the working tree is dirty without prior context, suggest running `start session`.
3. **Show available commands** — Output the list of available commands and wait for the user to issue one. Do not automatically run any command.

---

## Available Commands

### `start session`

Begin a new development session from a clean baseline.

**Process:**
1. `git checkout main`
2. `git pull --rebase`
3. Run `git status` to verify a clean working tree
4. Report the current commit hash and that the tree is ready for development

### `finish session`

Complete the current session: create a single atomic commit, rebase onto latest `main`, run tests, generate session evaluation, and push.

> **Ordering constraint**: Commit must be created *before* rebase so that rebase moves the single commit to the tip of main. The commit message must be obtained *before* the commit because it requires data from `generate` and `opencode session list`. The evaluation `.md` sidecar is written from the `generate` output (step 10) *after* the commit, so it reflects the final session state including any test-fix loops.

**Process:**
1. **Stage all changes**: `git add -A`
2. **Get evaluation title slug**: Load the `session-evaluation` skill via the `skill` tool, then instruct it to run `generate`. Extract the slug from the Session ID field of its output (e.g., `2026-05-11-testing-plan-revision`).
3. **Get session ID**: Run `opencode session list` and identify the most recent session. Strip the `ses_` prefix to get the noprefix ID (e.g., `1e793e9b0ffeLqAjZOHtI8vy8v`).
4. **Construct commit message**: `<title-slug>-<session-id-noprefix>` — this is identical to the session evaluation sidecar filename (e.g., `2026-05-11-testing-plan-revision-1e793e9b0ffeLqAjZOHtI8vy8v`).
5. **Create commit**: `git commit -m "<commit-message>"`
6. **Rebase onto main**: `git fetch origin && git rebase origin/main`
7. **Handle conflicts**: If conflicts occur:
   - For each conflicted file, resolve manually (edit to correct state)
   - `git add <resolved-files>`
   - `git rebase --continue`
   - If `git rebase --continue` opens an editor, save and exit immediately (the commit message from step 5 is preserved)
8. **Run tests**: `npx @opensassi/deepenc-harness build --test`. If that command is unavailable, run `ctest --test-dir build/release-shared --output-on-failure` directly.
9. **If tests fail**:
   - First determine if the failure is pre-existing (not caused by your session's changes). Verify by running the same test on a clean `main` checkout. If it fails there too, document it and proceed — do not loop fix-attempts on pre-existing failures.
   - If the failure is caused by your changes: fix the failing test(s) or code
   - `git add -A`
   - `git commit --amend --no-edit` (preserves the commit message)
   - Go back to step 6 (re-rebase onto latest main)
10. **Write evaluation sidecar**: Write the evaluation summary (produced by step 2's `generate` output) to `sessions/<title-slug>-<session-id-noprefix>.md` using the `write` tool.
11. **Export session archive**: Load the `session-evaluation` skill via the `skill` tool, then instruct it to run `export` with the title slug from step 2 and the session ID from step 3. This creates:
    - `sessions/<title-slug>-<session-id-noprefix>.md` — evaluation sidecar
    - `<title-slug>-<session-id-noprefix>.json.bz2` — compressed session JSON
    - `<title-slug>-<session-id-noprefix>.sha256` — content integrity hash
12. **Validate export artifacts**: Verify all three files are non-zero:
    ```
    ls -l sessions/<title-slug>-<session-id-noprefix>.md
    ls -l sessions/<title-slug>-<session-id-noprefix>.json.bz2
    ls -l sessions/<title-slug>-<session-id-noprefix>.sha256
    ```
    If any file is 0 bytes, re-run step 11. If the `.md` is missing, re-write it (the content was produced in step 2).
13. **Stage session artifacts**: `git add sessions/`
14. **Amend commit to include artifacts**: `git commit --amend --no-edit` (preserves the commit message, includes sidecar + archive files)
15. **Push**: `git push`

### `sync`

Fetch the latest changes from origin and rebase the current work onto them.

**Process:**
1. `git fetch origin`
2. `git rebase origin/main`
3. If conflicts: resolve → `git add` → `git rebase --continue`
4. Run `npx @opensassi/deepenc-harness build --test`
5. If tests fail: fix → `git add -A` → `git commit --amend --no-edit`
6. Report whether the sync completed cleanly or required intervention

---

## Design Principles

- **No commits during development** — All changes are staged via `git add -A` at `finish session` time. Never commit during the development phase.
- **Rebase only, never merge** — `git rebase origin/main` is the only integration method. Never use `git merge`.
- **Single atomic commit per session** — The commit message matches the session evaluation sidecar filename exactly. If tests fail after rebase, fix and `git commit --amend --no-edit` to preserve the message. Never add secondary fixup commits.
- **Full test suite after every rebase** — After rebasing, the complete test pipeline (`npx @opensassi/deepenc-harness build --test`) must pass before proceeding.
- **Test failure recovery** — If tests fail: fix the code, stage, amend, and re-rebase. Loop until the tests pass cleanly on top of the latest `main`.
- **Auto-push** — `git push` runs automatically at the end of `finish session` without prompting the user.
- **Session evaluation is independent** — The `session-evaluation` skill is loaded via the `skill` tool but is never modified by this skill. It handles `generate` and `export`; all git operations belong to this skill.
- **Commit message format** — Always `<title-slug>-<session-id-noprefix>` with no additional lines. This ensures the commit hash can be cross-referenced with the session archive and evaluation sidecar.
