#!/usr/bin/env bash
set -euo pipefail

part_dir=${1:?usage: scripts/merge_engine_match_parts.sh PART_DIR OUTPUT_PREFIX TOTAL_GAMES JOBS}
output_prefix=${2:?usage: scripts/merge_engine_match_parts.sh PART_DIR OUTPUT_PREFIX TOTAL_GAMES JOBS}
total_games=${3:?usage: scripts/merge_engine_match_parts.sh PART_DIR OUTPUT_PREFIX TOTAL_GAMES JOBS}
jobs=${4:?usage: scripts/merge_engine_match_parts.sh PART_DIR OUTPUT_PREFIX TOTAL_GAMES JOBS}

if [[ ! -d "$part_dir" ]]; then
    echo "part directory does not exist: $part_dir" >&2
    exit 1
fi

output_dir=$(dirname "$output_prefix")
if [[ "$output_dir" != "." ]]; then
    mkdir -p "$output_dir"
fi

games_per_job=$(( total_games / jobs ))
extra_games=$(( total_games % jobs ))

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
