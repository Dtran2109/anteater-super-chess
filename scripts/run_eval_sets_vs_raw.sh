#!/usr/bin/env bash
set -euo pipefail

output_prefix=${1:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
set1_name=${2:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
set1_pst=${3:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
set1_weights=${4:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
set2_name=${5:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
set2_pst=${6:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
set2_weights=${7:?usage: scripts/run_eval_sets_vs_raw.sh OUTPUT_PREFIX SET1_NAME SET1_PST SET1_WEIGHTS SET2_NAME SET2_PST SET2_WEIGHTS [games] [random_plies] [depth] [time_ms] [preset] [jobs] [tt_mb] [pst_scale] [learned_eval] [base_seed]}
games=${8:-90}
random_plies=${9:-6}
depth=${10:-10}
time_ms=${11:-1000}
preset=${12:-balanced}
jobs=${13:-30}
tt_mb=${14:-20480}
pst_scale=${15:-100}
learned_eval=${16:-0}
base_seed=${17:-23}

backup_dir=$(mktemp -d /tmp/anteater_eval_backup.XXXXXX)

restore_headers() {
    if [[ -f "$backup_dir/generated_piece_square_tables.h" ]]; then
        cp "$backup_dir/generated_piece_square_tables.h" src/generated_piece_square_tables.h
    fi

    if [[ -f "$backup_dir/generated_eval_weights.h" ]]; then
        cp "$backup_dir/generated_eval_weights.h" src/generated_eval_weights.h
    fi
}

trap restore_headers EXIT

cp src/generated_piece_square_tables.h "$backup_dir/generated_piece_square_tables.h"
cp src/generated_eval_weights.h "$backup_dir/generated_eval_weights.h"

run_set() {
    local name=$1
    local pst=$2
    local weights=$3
    local out="${output_prefix}_${name}"

    if [[ ! -f "$pst" ]]; then
        echo "PST file does not exist for ${name}: ${pst}" >&2
        exit 1
    fi

    cp "$pst" src/generated_piece_square_tables.h

    if [[ "$weights" != "none" ]]; then
        if [[ ! -f "$weights" ]]; then
            echo "weight file does not exist for ${name}: ${weights}" >&2
            exit 1
        fi

        cp "$weights" src/generated_eval_weights.h
    fi

    echo "Running eval set '${name}' vs raw alpha-beta"
    ./scripts/run_engine_match_parallel.sh "$out" "$games" "$random_plies" "$depth" "$time_ms" "$preset" "$jobs" "$tt_mb" raw "$pst_scale" "$learned_eval" "$base_seed"
}

run_set "$set1_name" "$set1_pst" "$set1_weights"
run_set "$set2_name" "$set2_pst" "$set2_weights"

restore_headers
trap - EXIT

echo "Restored original generated eval headers."
