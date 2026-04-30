#!/usr/bin/env bash
set -euo pipefail

output_prefix=${1:-engine_match_parallel}
total_games=${2:-100}
random_opening_plies=${3:-6}
depth=${4:-10}
time_ms=${5:-1000}
preset=${6:-fast}
jobs=${7:-20}
total_tt_mb=${8:-20480}
match_suite=${9:-all}
current_pst_scale=${10:-0}
learned_eval=${11:-0}
base_seed=${12:-23}

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

output_dir=$(dirname "$output_prefix")
if [[ "$output_dir" != "." ]]; then
    mkdir -p "$output_dir"
fi

make engine_match

run_id=$(date +%Y%m%d_%H%M%S)
part_dir="engine_match_parts/${run_id}"
mkdir -p "$part_dir"

games_per_job=$(( total_games / jobs ))
extra_games=$(( total_games % jobs ))
tt_mb_per_job=$(( total_tt_mb / jobs ))
if (( tt_mb_per_job <= 0 )); then
    tt_mb_per_job=1
fi

echo "Writing ${total_games} games per matchup to ${output_prefix}_*.csv"
echo "Workers: ${jobs}, depth=${depth}, time=${time_ms} ms, preset=${preset}, random_opening=${random_opening_plies}"
echo "Suite: ${match_suite}, current_pst=${current_pst_scale}, learned_eval=${learned_eval}"
echo "Base seed: ${base_seed}"
echo "TT budget: ${total_tt_mb} MB total, ${tt_mb_per_job} MB per worker"
echo "Parts: ${part_dir}"

pids=()

for ((i = 0; i < jobs; i++)); do
    games_for_job=$games_per_job
    if (( i < extra_games )); then
        games_for_job=$(( games_for_job + 1 ))
    fi

    part_prefix=$(printf "%s/part_%02d" "$part_dir" "$i")
    seed=$(( base_seed + i ))

    ./bin/engine_match "$games_for_job" "$random_opening_plies" "$part_prefix" "$depth" "$time_ms" "$preset" "$tt_mb_per_job" "$match_suite" "$current_pst_scale" "$learned_eval" "$seed" \
        >"${part_prefix}.log" 2>&1 &
    pids+=("$!")
done

failed=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        failed=1
    fi
done

if (( failed != 0 )); then
    echo "At least one engine-match worker failed. Check ${part_dir}/part_*.log" >&2
    exit 1
fi

first_part=$(printf "%s/part_%02d" "$part_dir" 0)
head -n 1 "${first_part}_games.csv" > "${output_prefix}_games.csv"
head -n 1 "${first_part}_moves.csv" > "${output_prefix}_moves.csv"

for ((i = 0; i < jobs; i++)); do
    part_prefix=$(printf "%s/part_%02d" "$part_dir" "$i")
    game_offset=$(( i * games_per_job + (i < extra_games ? i : extra_games) ))

    awk -F, -v OFS=, -v offset="$game_offset" 'NR > 1 {$2 += offset; print}' "${part_prefix}_games.csv" \
        >> "${output_prefix}_games.csv"
    awk -F, -v OFS=, -v offset="$game_offset" 'NR > 1 {$2 += offset; print}' "${part_prefix}_moves.csv" \
        >> "${output_prefix}_moves.csv"
done

python3 src/summarize_matches.py \
    --games "${output_prefix}_games.csv" \
    --moves "${output_prefix}_moves.csv" \
    > "${output_prefix}_summary.txt"

game_rows=$(( $(wc -l < "${output_prefix}_games.csv") - 1 ))
move_rows=$(( $(wc -l < "${output_prefix}_moves.csv") - 1 ))

echo "Merged ${game_rows} game rows into ${output_prefix}_games.csv"
echo "Merged ${move_rows} move rows into ${output_prefix}_moves.csv"
echo "Summary written to ${output_prefix}_summary.txt"
