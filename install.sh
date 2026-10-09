#!/bin/bash
# Stable installation entry point; implementation is in scripts/.
QUALPF_REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$QUALPF_REPO_DIR/scripts/install.sh" "$@"
