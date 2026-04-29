#!/usr/bin/env bash
set -euo pipefail

output=${1:-training_data.csv}
total_games=${2:-3000}
max_plies=${3:-120}
depth=${4:-10}
time_ms=${5:-1000}
preset=${6:-fast}
jobs=${7:-30}
base_seed=${8:-23}
total_tt_mb=${9:-20480}

if (( total_games <= 0 )); then
    echo "total_games must be positive" >&2
    exit 1
fi

if (( jobs <= 0 )); then
    echo "jobs must be positive" >&2
    exit 1
fi

if (( jobs > total_games )); then
    jobs=$total_games
fi

make selfplay

run_id=$(date +%Y%m%d_%H%M%S)
part_dir="selfplay_parts/${run_id}"
mkdir -p "$part_dir"

games_per_job=$(( total_games / jobs ))
extra_games=$(( total_games % jobs ))
tt_mb_per_job=$(( total_tt_mb / jobs ))
if (( tt_mb_per_job <= 0 )); then
    tt_mb_per_job=1
fi

echo "Writing ${total_games} games to ${output}"
echo "Workers: ${jobs}, depth=${depth}, time=${time_ms} ms, preset=${preset}, max_plies=${max_plies}"
echo "TT budget: ${total_tt_mb} MB total, ${tt_mb_per_job} MB per worker"
echo "Parts: ${part_dir}"

pids=()

for ((i = 0; i < jobs; i++)); do
    games_for_job=$games_per_job
    if (( i < extra_games )); then
        games_for_job=$(( games_for_job + 1 ))
    fi

    part_file=$(printf "%s/part_%02d.csv" "$part_dir" "$i")
    seed=$(( base_seed + i ))
    game_id_start=$(( i * games_per_job + (i < extra_games ? i : extra_games) + 1 ))

    ./bin/selfplay_data "$part_file" "$games_for_job" "$max_plies" "$depth" "$time_ms" "$preset" "$seed" "$game_id_start" "$tt_mb_per_job" \
        >"${part_file%.csv}.log" 2>&1 &
    pids+=("$!")
done

failed=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        failed=1
    fi
done

if (( failed != 0 )); then
    echo "At least one self-play worker failed. Check ${part_dir}/part_*.log" >&2
    exit 1
fi

first_part=$(printf "%s/part_%02d.csv" "$part_dir" 0)
head -n 1 "$first_part" > "$output"

for ((i = 0; i < jobs; i++)); do
    part_file=$(printf "%s/part_%02d.csv" "$part_dir" "$i")
    tail -n +2 "$part_file" >> "$output"
done

rows=$(( $(wc -l < "$output") - 1 ))
echo "Merged ${rows} samples into ${output}"
