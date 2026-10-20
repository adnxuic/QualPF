#!/usr/bin/env python3
"""Compatibility entry point; use pfsm_candidate_recall_compare."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pfsm_candidate_recall_compare import *  # noqa: F401,F403

if __name__ == "__main__":
    raise SystemExit(main())
