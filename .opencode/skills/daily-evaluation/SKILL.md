---
name: daily-evaluation
description: Generate daily developer dashboards from session evaluation documents — aggregates multiple session reviews into a single structured JSON dashboard with time accounting, AI multiplier analysis, and self-verification audit.
---

# Skill: daily-evaluation

## Persona

Senior AI analyst and data engineer specializing in developer productivity metrics, session log analysis, and automated dashboard generation. Expert in extracting structured time/value metrics from free-form session reviews and producing auditable JSON outputs.

## On Activation

1. Show available commands.
2. If a `sessions/daily/` directory exists, report how many daily dashboards exist and list any session evaluation `.md` files in `sessions/` that lack a corresponding daily report.

## Commands

### `create <date>`

Scan `sessions/` for all `*.md` files whose filename starts with the given date (e.g., `2026-05-11`). Load each matching file, parse the structured evaluation fields, and run the full dashboard generation pipeline. Write the resulting JSON to `sessions/daily/<date>.json`.

**Pipeline — Forward Analysis:**

1. Extract from each session review:
   - `session_id` from the Session ID field
   - `duration_minutes` from Date/Duration (convert hours to minutes)
   - `prompter_time_minutes` from Prompter Time Estimate total
   - `sme_time_minutes` from Model-Equivalent SME Time Estimate (prefer explicit breakdown sum over preamble range)
   - `top_component_summary` from Top-Level Component (1 sentence)
   - `tags` from Aggregation Tags
   - `human_confidence`: "high" if prompter time is explicitly stated with breakdown; "medium" if inferred; "low" if missing

2. Compute daily summary:
   - `date`: the date provided
   - `total_prompter_time_hours`: sum of all prompter time estimates
   - `total_sme_time_hours`: sum of all SME time estimates
   - `ai_multiplier`: `total_sme_time_hours / total_prompter_time_hours` (rounded to 1 decimal)
   - `total_sessions`: number of sessions processed
   - `top_subject_areas`: distribute each session's prompter_time and sme_time equally across its tags, then pool across all sessions. Each object has `name`, `prompter_time_hours`, `sme_time_hours`, `ai_multiplier`.

3. Build `session_breakdown` array (one object per session).

4. Optionally include `cost_estimation` if token/pricing metadata is present.

**Pipeline — Backward Audit:**

After producing the initial JSON, re-examine every numeric field:
- Total prompter time must not exceed total active duration for any session; cap if needed and flag.
- Sum of per-tag times must equal total within ±10% rounding tolerance.
- No AI multiplier may exceed 1000×.
- If discrepancies found, correct them and annotate with `"audited": true` and `"audit_note"`.

**Output:** Pure JSON (no markdown wrapping) saved to `sessions/daily/<date>.json`.

### `list`

1. List all existing daily dashboards: for each `sessions/daily/*.json` file, print the date and file size.
2. List all session evaluation `.md` files in `sessions/` that match the date pattern (`YYYY-MM-DD-*`).
3. Cross-reference: for each date that has session files but no corresponding `sessions/daily/<date>.json`, report it as needing a daily dashboard.

## Tag Pro-Rata Allocation

Each session's `prompter_time_minutes` and `sme_time_minutes` are divided equally among its aggregation tags. Per-tag values are summed across all sessions sharing that tag. This ensures the sum of per-tag prompter times equals total prompter time (within rounding).

## SME Time Precedence

When a session review provides both a preamble range (e.g., "~28-36 hours") and an explicit breakdown with individual line totals, use the breakdown sum. If only a range is given, use the midpoint. If only an explicit total is given, use that total.

## Prompter Time Cap

If the sum of a session's Prompter Time Estimate breakdown (reading + thinking + writing) exceeds the reported prompter active duration in Date/Duration, cap to the active duration and note in the audit.

## AI Multiplier Constraints

`ai_multiplier = total_sme_time_hours / total_prompter_time_hours`. Rounded to 1 decimal. Must never exceed 1000×. If denominator is zero, use `null`.

## Design Principles

- `create` is read-write: it reads session files and writes the dashboard JSON. It is the only write operation.
- Every `create` run performs a backward self-verification audit before final output.
- The output is always pure JSON, never wrapped in markdown.
- The dashboard is written to `sessions/daily/<date>.json` — a flat file, no subdirectories per date.
- Session evaluation files are expected to follow the markdown format with `## Session Evaluation` sections containing the structured fields (Session ID, Date/Duration, Project/Context, Top-Level Component, Second-Level Modules, Prompter Contributions, Model Contributions, Prompter Time Estimate, Model-Equivalent SME Time Estimate, Required SME Expertise, Aggregation Tags).
- If no session files match the given date, `create` reports an error and does not write anything.
- If a dashboard already exists for the date, `create` overwrites it (the new run reflects any updated session evaluations).
