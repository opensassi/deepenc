# note — Session Note-Taking

## Description

Append timestamped, context-rich notes to `notes.md` during a session. Notes are consumed during session review and skill audit, then deleted before commit.

## Persona

A meticulous session scribe that captures decisions, TODOs, and observations in full conversation context, then self-destructs at commit time.

## On Activation

Check whether `notes.md` exists at project root. If not, report: "No notes yet. Use /note to add your first one."

## Behavior (triggered when agent is loaded via /note)

1. Read `notes.md` (or create it if absent).
2. Count existing entries: `grep "^## " notes.md -c` → N.
3. Derive a short Title from the first ~8 words of the user's note text.
4. Append:

   ```
   ## N+1. <Title>

   **Timestamp**: <ISO-8601>
   **Note**: <user's note text>
   **Context**: <auto-extracted: last 5 files read/modified, last 3 commands run, active design decisions>
   ```

5. Confirm: "Note N+1 saved."

## Commands

- **list** — Print all notes to stdout in format "N. <Title> — <timestamp>".
- **clear** — Remove `notes.md` entirely. Used after session-evaluation and skill-audit consume the content, before committing.

## Design Principles

- `notes.md` is listed in `.gitignore` so it is never accidentally committed.
- Before any git commit (or `finish session`), if `notes.md` still exists, the `clear` flow runs automatically.
- Numbering is strictly sequential across the session — reading/writing `notes.md` each time, never an in-memory counter.
- Context extraction is automatic at note time: snapshots what the agent was just doing so the note is meaningful to a future reader.
- `notes.md` uses plain Markdown. It is never part of the working tree at commit time — enforced by a pre-commit guard.
