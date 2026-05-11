#!/usr/bin/env bash
set -e

SESSIONS_DIR="$(cd "$(dirname "$0")" && pwd)"
TITLE_SLUG="$1"
SESSION_ID="$2"

if [ -z "$TITLE_SLUG" ] || [ -z "$SESSION_ID" ]; then
  echo "Usage: export-session.sh <title-slug> <session-id>"
  echo ""
  echo "Export an opencode session to the sessions/ directory."
  echo ""
  echo "Arguments:"
  echo "  <title-slug>   Lower-dash-case title (e.g. 2026-05-11-deepenc-setup)"
  echo "  <session-id>   opencode session ID (e.g. ses_1e8288...)"
  echo ""
  echo "Produces:"
  echo "  sessions/<title-slug>-<session-id-noprefix>.json   full opencode export"
  echo "  sessions/<title-slug>-<session-id-noprefix>.sha256  content hash"
  echo ""
  echo "Example:"
  echo "  sessions/export-session.sh 2026-05-11-my-session ses_abc123"
  exit 1
fi

SESSION_ID_NO_PREFIX="${SESSION_ID#ses_}"
JSON_FILE="${SESSIONS_DIR}/${TITLE_SLUG}-${SESSION_ID_NO_PREFIX}.json"
SHA_FILE="${SESSIONS_DIR}/${TITLE_SLUG}-${SESSION_ID_NO_PREFIX}.sha256"

echo "=> Exporting session ${SESSION_ID}..."
opencode export "$SESSION_ID" 2>/dev/null > "$JSON_FILE"
echo "   Saved: ${JSON_FILE}"

echo "=> Computing content hash..."
sha256sum "$JSON_FILE" | cut -d' ' -f1 > "$SHA_FILE"
echo "   Hash:  $(cat "$SHA_FILE")"

wc -c < "$JSON_FILE" | xargs -I{} echo "=> Done ({} bytes)"
