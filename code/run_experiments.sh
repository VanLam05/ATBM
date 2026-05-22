#!/bin/bash
# =======================================================
# Run all experiments for Influence Minimization
# =======================================================
# This script:
# 1. Compiles all code
# 2. Prepares datasets for IMin format
# 3. Runs AdvancedGreedy (AG), GreedyReplace (GR) experiments
# 4. Runs IMin (SandIMIN) experiments
#
# Usage: bash run_experiments.sh [small|full]
#   small: quick test with small parameters
#   full:  full experiment (default)
# =======================================================

set -e

MODE=${1:-full}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DATA_DIR="$SCRIPT_DIR/../datasets"
RESULTS_DIR="$SCRIPT_DIR/../results"
mkdir -p "$RESULTS_DIR"

echo "=== Mode: $MODE ==="
echo "=== Script dir: $SCRIPT_DIR ==="
echo "=== Data dir: $DATA_DIR ==="
echo "=== Results dir: $RESULTS_DIR ==="

# -------------------------------------------------------
# 1. Compile
# -------------------------------------------------------
echo ""
echo "=== Compiling ==="

cd "$SCRIPT_DIR/AdvancedGreedy"
g++ -O3 -std=c++17 -o ag advanced_greedy.cpp
g++ -O3 -std=c++17 -o experiment experiment.cpp
echo "  Compiled AdvancedGreedy and experiment runner"

cd "$SCRIPT_DIR/IMin"
g++ -O3 -std=c++17 -o prepare_dataset prepare_dataset.cpp
echo "  Compiled prepare_dataset"

cd "$SCRIPT_DIR/IMin/SandIMIN_code"
IMIN_COMPILED=1
if g++ -O3 -o IMIN Sandwich.cpp sfmt/SFMT.c 2>/dev/null; then
    echo "  Compiled IMin (SandIMIN)"
elif g++ -O3 -std=c++17 -o IMIN Sandwich.cpp sfmt/SFMT.c -x c++ 2>/dev/null; then
    echo "  Compiled IMin (SandIMIN) with c++17"
else
    echo "  [WARNING] IMin compilation failed - IMin experiments will be skipped"
    echo "  Try: cd IMin/SandIMIN_code && g++ -O3 -o IMIN Sandwich.cpp sfmt/SFMT.c"
    IMIN_COMPILED=0
fi

# -------------------------------------------------------
# 2. Dataset preparation note
# -------------------------------------------------------
echo ""
echo "=== Dataset preparation ==="

DATASETS=("p2p-Gnutella31.txt" "email-EuAll.txt" "com-dblp.ungraph.txt" "com-youtube.ungraph.txt")
DATASET_NAMES=("p2p-Gnutella31" "email-EuAll" "com-dblp" "com-youtube")

echo "  IMin now reads the common dataset files directly from $DATA_DIR"
echo "  prepare_dataset is kept only for legacy SandIMIN-format runs."

# -------------------------------------------------------
# 3. Set parameters
# -------------------------------------------------------
if [ "$MODE" == "small" ]; then
    THETA=10
    MC=1000
    TIME_LIMIT=60
    K_VALUES="5"
    S_VALUES="10"
    echo ""
    echo "=== Small test mode: theta=$THETA, mc=$MC, timeLimit=$TIME_LIMIT ==="
else
    THETA=100
    MC=10000
    TIME_LIMIT=100000
    K_VALUES="100 200 300 400 500"
    S_VALUES="10 20 30 40 50"
    echo ""
    echo "=== Full experiment mode: theta=$THETA, mc=$MC, timeLimit=$TIME_LIMIT ==="
fi

# -------------------------------------------------------
# 4. Run AG/GR experiments
# -------------------------------------------------------
echo ""
echo "=== Running AdvancedGreedy / GreedyReplace experiments ==="

AG_LOG="$RESULTS_DIR/ag_gr_results.txt"
echo "# AG/GR Experiment Results" > "$AG_LOG"
echo "# $(date)" >> "$AG_LOG"
echo "# theta=$THETA mc=$MC timeLimit=$TIME_LIMIT" >> "$AG_LOG"

cd "$SCRIPT_DIR/AdvancedGreedy"
./experiment -datadir "$DATA_DIR" -theta $THETA -mc $MC -timeLimit $TIME_LIMIT -log "$AG_LOG" 2>&1 | tee "$RESULTS_DIR/ag_gr_console.log"

# -------------------------------------------------------
# 5. Run IMin experiments
# -------------------------------------------------------
echo ""
echo "=== Running IMin (SandIMIN) experiments ==="

if [ "$IMIN_COMPILED" -eq 0 ]; then
    echo "  [SKIP] IMin was not compiled successfully. Skipping IMin experiments."
else

IMIN_LOG="$RESULTS_DIR/imin_results.txt"
echo "# IMin Experiment Results" > "$IMIN_LOG"
echo "# $(date)" >> "$IMIN_LOG"

cd "$SCRIPT_DIR/IMin/SandIMIN_code"

DAY_LIMIT=86400

for i in "${!DATASETS[@]}"; do
    ds="${DATASETS[$i]}"
    name="${DATASET_NAMES[$i]}"
    ds_path="$DATA_DIR/$ds"
    seed_base="${ds%.*}"
    if [ ! -f "$ds_path" ]; then
        echo "  [SKIP] $name: dataset file not found at $ds_path"
        continue
    fi

    echo ""
    echo "--- Dataset: $name ---"

    for k in $K_VALUES; do
        for s in $S_VALUES; do
            echo "  Running IMin: k=$k |S|=$s on $name"
            seed_file="$DATA_DIR/${seed_base}_seed_${s}.txt"

            # Estimate time with small run first
            START_TIME=$(date +%s)
            timeout 30 ./IMIN -dataset "$ds_path" -k 1 -rumorNum $s -algo SandIMIN \
                -epsilon 0.2 -gamma 0.1 -beta 0.1 -seedFile "$seed_file" -outputDir "$RESULTS_DIR" > /tmp/imin_est.log 2>&1 || true
            END_TIME=$(date +%s)
            EST_PER_ITER=$((END_TIME - START_TIME))
            EST_TOTAL=$((EST_PER_ITER * k))

            if [ $EST_TOTAL -gt $DAY_LIMIT ]; then
                echo "  [SKIP] Estimated time ${EST_TOTAL}s > 1 day. Logging and skipping."
                echo "SKIP	$name	k=$k	|S|=$s	estimated=${EST_TOTAL}s	>1day" >> "$IMIN_LOG"
                continue
            fi

            EXIT_CODE=0
            timeout $TIME_LIMIT ./IMIN -dataset "$ds_path" -k $k -rumorNum $s -algo SandIMIN \
                -epsilon 0.2 -gamma 0.1 -beta 0.1 -seedFile "$seed_file" -outputDir "$RESULTS_DIR" > /tmp/imin_run.log 2>&1 || EXIT_CODE=$?
            if [ $EXIT_CODE -eq 124 ]; then
                echo "  [TIMEOUT] IMin timed out after ${TIME_LIMIT}s"
                echo "TIMEOUT	$name	k=$k	|S|=$s	timeLimit=${TIME_LIMIT}s" >> "$IMIN_LOG"
            else
                echo "  [DONE] IMin completed"
                cat /tmp/imin_run.log >> "$IMIN_LOG"
                echo "---" >> "$IMIN_LOG"
            fi

            # IMin writes directly to $RESULTS_DIR via -outputDir.
        done
    done
done

fi # end IMIN_COMPILED check

# -------------------------------------------------------
# 6. Generate table-ready summaries
# -------------------------------------------------------
echo ""
echo "=== Generating table-ready CSV summaries ==="
cd "$SCRIPT_DIR"
python3 summarize_results.py --results "$RESULTS_DIR"

echo ""
echo "=== All experiments completed ==="
echo "Results saved in: $RESULTS_DIR/"
echo "  - ag_gr_results.txt: AdvancedGreedy / GreedyReplace results"
echo "  - ag_gr_console.log: AG/GR console output"
echo "  - imin_results.txt: IMin results"
echo "  - table_k_saved.csv / table_k_time.csv: tables for varying k"
echo "  - table_s_saved.csv / table_s_time.csv: tables for varying |S|"
echo "  - table_approx_ratio.csv: approximation-ratio table when available"
