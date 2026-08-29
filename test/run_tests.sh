#!/usr/bin/env bash
#
# run_tests.sh — Tail test harness.
#
# Runs each .tail program in test/cases/ and compares its stdout against the
# expected output stored in the corresponding .out file. No external test
# framework is required.
#
# SPDX-License-Identifier: MIT

set -u

BIN="${BIN:-./tail}"
CASES_DIR="$(cd "$(dirname "$0")" && pwd)/cases"

pass=0
fail=0

if [[ ! -x "$BIN" ]]; then
    echo "error: $BIN not found or not executable (run 'make' first)" >&2
    exit 1
fi

for src in "$CASES_DIR"/*.tail; do
    name="$(basename "$src" .tail)"
    expected="$CASES_DIR/$name.out"

    if [[ ! -f "$expected" ]]; then
        echo "SKIP  $name (no $name.out)"
        continue
    fi

    actual="$("$BIN" "$src" 2>&1)"
    rc=$?

    if [[ $rc -ne 0 ]]; then
        echo "FAIL  $name (exit $rc)"
        echo "      stderr: $actual"
        fail=$((fail + 1))
        continue
    fi

    if [[ "$actual" == "$(cat "$expected")" ]]; then
        echo "PASS  $name"
        pass=$((pass + 1))
    else
        echo "FAIL  $name"
        echo "--- expected ---"
        cat "$expected"
        echo "--- actual ---"
        echo "$actual"
        fail=$((fail + 1))
    fi
done

echo
echo "passed: $pass  failed: $fail"

if [[ $fail -gt 0 ]]; then
    exit 1
fi
exit 0
