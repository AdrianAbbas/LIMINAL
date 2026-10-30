#!/usr/bin/env bash
# Runs the whole Linux test suite as concurrent shards, then the CPU-measuring group
# alone so its timing is not disturbed. Together the shards cover every test group.
# (Adapted from WORLDBUILDER.)
# Usage: scripts/run-tests-parallel.sh path/to/LiminalTests [log directory]
set -u
tests="${1:?the LiminalTests binary}"
logs="${2:-build/test-logs}"
mkdir -p "$logs"

"$tests" --only parameters,smoke,power,state,choir,box > "$logs/1-core.txt" 2>&1 &
"$tests" --only midi,piano,synth,glass > "$logs/2-midi.txt" 2>&1 &
"$tests" --only levels,rates,determinism,instruments,drone > "$logs/3-levels.txt" 2>&1 &
"$tests" --only editor --snapshot "$logs/editor.png" > "$logs/4-editor.txt" 2>&1 &
"$tests" --only environments > "$logs/6-environments.txt" 2>&1 &
wait
"$tests" --only realtime > "$logs/5-cpu.txt" 2>&1

passed=0
failed=0
for log in "$logs"/[1-6]-*.txt; do
    line=$(grep -E '^[0-9]+ passed, [0-9]+ failed' "$log" | tail -1)
    if [[ -z "$line" ]]; then
        echo "$log: did not finish"
        tail -20 "$log"
        failed=$((failed + 1))
        continue
    fi
    passed=$((passed + $(echo "$line" | cut -d' ' -f1)))
    failed=$((failed + $(echo "$line" | cut -d' ' -f3)))
    grep -E '^FAIL|\[metric\]' "$log"
done
echo "$passed passed, $failed failed"
[[ "$failed" -eq 0 ]]
