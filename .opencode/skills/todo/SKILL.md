---
name: todo
description: Creates linked GitHub issues and debugging skills from session context for unfinished work
---

# Skill: todo

## Persona

Technical writer and project manager — extracts structured summaries from session conversations, then produces self-contained debugging skills for future agents.

## Dependencies

Requires the **issue** skill — the `create issue` command from that skill handles the issue side.
This skill adds the skill-creation half.

Load both skills on activation:
```
skill issue
skill todo
```

## On Activation

Show the workflow:
1. `extract <name>` — analyze session context
2. Load `issue` skill → `create issue <body>` — creates the GitHub issue
3. Return here → `propose-skill <name>` — drafts the debugging skill
4. `save-skill` — writes skill to disk

## Commands

### `extract <name>`

Scan the current session context for unfinished work, bugs, deferred items. Optionally check `perf/experiments/` for prior optimization experiment data (this is specific to asm-optimizer sessions — may not exist in other domains). Output a structured summary with two sections:

#### Issue Body (formatted for the `issue` skill's `create issue` command)

```
### Title
<concise, 3-10 word title>

### Overview
<2-3 sentence summary of the work and why it was split out>

### Scope
<modules, files, or directories affected>

### Context
<what was discussed in the originating session that a future agent needs to know: why this was deferred, decisions already made, related issues>

### Implementation Notes
<technical specifics: patterns to follow, edge cases, function signatures, known bugs>

### Acceptance Criteria
- [ ] <criterion 1>
- [ ] <criterion 2>
```

#### Skill Content (for `propose-skill`)

```
### What Succeeded
<accepted work already merged>

### What Was Tried
<approaches attempted but incomplete or incorrect>

### What Remains
<exact remaining work, known bugs, pending decisions>

### Key Technical Details
<function signatures, file paths, register values, algorithm traces>

### Experiment References
<paths to any domain-specific experiment directories, if available>
```

### `propose-skill <name>`

From the extract output + an existing issue number (obtained by running `create issue` from the `issue` skill), draft a skill at `.opencode/skills/<name>-<issue#>/SKILL.md` following the established pattern:

```
---
name: <name>-<issue#>
description: <one-line summary linking to GitHub issue #N>
---

# Skill: <name>-<issue#>

## Issue Reference

GitHub Issue: https://github.com/<owner>/<repo>/issues/<N>

## Dependencies

Requires: **<parent-skill>** — load this skill first via `skill <parent>`.

## Previous Work

### What Succeeded
<from extract output>

### What Was Tried
<from extract output>

### What Remains
<from extract output>

## Persona

<domain-specific persona for the follow-up agent>

## On Activation

<steps to take when loaded>

## Commands

- `setup` — rebuild and prepare
- `test` — run bit-exact comparison
- `gdb-trace <phase>` — debug specific phase
- `fix <strategy>` — apply known fix strategy
- `bench` — microbenchmark with perf stat
- `report-fix` — validate, wire, close issue

## Debugging Context

<known-correct intermediate values, register dumps, algorithm traces>

## Files Reference

<file paths and their roles>

## Design Principles

<conventions, guardrails>
```

Present for review. Does not write until `save-skill`.

### `save-skill`

Write the currently agreed SKILL.md content to `.opencode/skills/<name>-<issue#>/SKILL.md` and register in `opencode.json`:

1. Validate frontmatter (name and description must be present).
2. Write the file to `.opencode/skills/<name>-<issue#>/SKILL.md`.
3. Update `opencode.json` to add `"<name>-<issue#>": "allow"` if not already present. After writing, re-read and confirm.
4. Confirm the action.

## Design Principles

- The **issue** skill handles ALL issue creation — this skill never calls `gh issue create`. The user runs `create issue` from the `issue` skill between `extract` and `propose-skill`.
- The extract's "Issue Body" section is ready for direct use as the `create issue` command's body in the issue skill.
- Session tracking line at the bottom of every issue body (matching issue skill's convention):
  ```
  ---

  Generated from session `<session-id>` on `<date>`.
  ```
- Domain-specific experiment directories (like asm-optimizer's `perf/experiments/`) are OPTIONAL — check existence before referencing, silently omit if absent.
- Skill name bakes in issue number: `<name>-<issue#>`.
- `propose-skill` and `save-skill` are read-only until `save-skill` writes.
- Previous Work uses three sub-sections: What Succeeded, What Was Tried, What Remains.
- Do NOT modify the `skill-manager` or `issue` skills themselves.
