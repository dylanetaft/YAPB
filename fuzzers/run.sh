#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
"${SCRIPT_DIR}/../build/fuzzers/fuzz_parse" -workers=64 -jobs=64 -timeout=120 "${SCRIPT_DIR}/corpus"
