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
"$tests" --only effects > "$logs/7-effects.txt" 2>&1 &
"$tests" --only characters > "$logs/8-characters.txt" 2>&1 &
"$tests" --only rules > "$logs/11-rules.txt" 2>&1 &
"$tests" --only distinct > "$logs/12-distinct.txt" 2>&1 &
# 2026-11-02: the scrolling lists (preset browser) at every window size.
"$tests" --only scroll > "$logs/19-scroll.txt" 2>&1 &
# Milestone 8: the instruments on the developer's recordings (stand-ins only without them).
"$tests" --only recorded > "$logs/18-recorded.txt" 2>&1 &
# 2026-11-02: the instrument library (twenty instruments in six slots) and old sessions unchanged.
"$tests" --only library > "$logs/20-library.txt" 2>&1 &
"$tests" --only sessions > "$logs/21-sessions.txt" 2>&1 &
"$tests" --only icons > "$logs/22-icons.txt" 2>&1 &
"$tests" --only library-rooms > "$logs/23-library-rooms.txt" 2>&1 &
wait
# Presets, MIDI, quality, export (it times Eco against Standard: alone, so the other shards do not disturb it).
"$tests" --only workflow > "$logs/9-workflow.txt" 2>&1
# The VHS look (its editor checks run in real time), then the editor's CPU in normal playing per
# VHS amount, each alone so their timing is not disturbed.
"$tests" --only vhs > "$logs/13-vhs.txt" 2>&1
"$tests" --only realtime > "$logs/5-cpu.txt" 2>&1
# The editor's frames in real time (and its CPU), also alone so the timing is not disturbed.
"$tests" --only frames > "$logs/10-frames.txt" 2>&1
"$tests" --only cpu > "$logs/14-cpu.txt" 2>&1
"$tests" --only convolver > "$logs/15-convolver.txt" 2>&1
"$tests" --only captures > "$logs/16-captures.txt" 2>&1
# Liminality (2026-11-01): includes its CPU measure, so alone too.
"$tests" --only liminality > "$logs/17-liminality.txt" 2>&1

passed=0
failed=0
for log in "$logs"/[1-9]-*.txt "$logs"/[12][0-9]-*.txt; do
    line=$(grep -E '^[0-9]+ passed, [0-9]+ failed' "$log" | tail -1)
    if [[ -z "$line" ]]; then
        echo "$log: did not finish"
        tail -20 "$log"
        failed=$((failed + 1))
        continue
    fi
    passed=$((passed + $(echo "$line" | cut -d' ' -f1)))
    failed=$((failed + $(echo "$line" | cut -d' ' -f3)))
    grep -E '^FAIL|^MISS|\[metric\]' "$log"
done
echo "$passed passed, $failed failed"
[[ "$failed" -eq 0 ]]
