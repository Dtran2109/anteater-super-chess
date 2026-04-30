#!/usr/bin/env bash
set -euo pipefail

workers=${WORKERS:-50}
ram_percent=${RAM_PERCENT:-80}

threads=$(nproc)
mem_available_kb=$(awk '/MemAvailable:/ {print $2}' /proc/meminfo)
mem_total_kb=$(awk '/MemTotal:/ {print $2}' /proc/meminfo)

if (( workers > threads )); then
    workers=$threads
fi

tt_mb=$(( mem_available_kb * ram_percent / 100 / 1024 ))
tt_per_worker_mb=$(( tt_mb / workers ))
mem_total_gib=$(( mem_total_kb / 1024 / 1024 ))
mem_available_gib=$(( mem_available_kb / 1024 / 1024 ))

cat <<EOF
Server profile
==============
Threads visible:       ${threads}
Workers selected:      ${workers}
RAM total:             ${mem_total_gib} GiB
RAM available:         ${mem_available_gib} GiB
TT RAM budget:         ${tt_mb} MB (${ram_percent}% of available)
TT per worker:         ${tt_per_worker_mb} MB

Self-play balanced data:
./scripts/run_selfplay_parallel.sh training_balanced.csv 1800 120 10 1000 balanced ${workers} 23 ${tt_mb}

Engine match, raw suite:
./scripts/run_engine_match_parallel.sh test/server_raw_90 90 6 10 1000 balanced ${workers} ${tt_mb} raw 0 0 23

Engine match, all suites:
./scripts/run_engine_match_parallel.sh test/server_all_90 90 6 10 1000 balanced ${workers} ${tt_mb} all 0 0 23

Eval set comparison vs raw:
./scripts/run_eval_sets_vs_raw.sh test/eval_compare one PATH_TO_ONE_PST none two PATH_TO_TWO_PST none 90 6 10 1000 balanced ${workers} ${tt_mb} 100 0 23

Override example:
WORKERS=40 RAM_PERCENT=70 ./scripts/server_config.sh
EOF
