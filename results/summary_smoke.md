# Smoke experiment results

Source script: `docs/Kich_ban.pdf`

Run date: 2026-05-17

Dataset: `p2p-Gnutella31`

Seed policy: chọn ngẫu nhiên `|S|` nút từ top-200 out-degree bằng RNG cố định. Tập seed dùng chung cho AG và IMin, chỉ khác thứ tự trong file:

```text
850 2450 12145 13876 17033 17584 27703 32144 50657 61196
```

Parameters for this smoke run:

| Parameter | Value |
|---|---:|
| `|S|` | 10 |
| `k` | 5, 100 |
| AG `theta` | 10 |
| AG MC rounds | 1000 |
| IMin `epsilon` | 0.2 |
| IMin `gamma` | 0.1 |
| IMin `beta` | 0.1 |

Note: this is a trial run, not the full scenario. The full scenario in the PDF uses `k = 100, 200, 300, 400, 500`, `|S| = 10, 20, 30, 40, 50`, and larger AG sampling parameters.

## Baseline results

| Algorithm | k | Spread before | Spread after | Saved nodes | Time (s) | Blockers/result |
|---|---:|---:|---:|---:|---:|---|
| AdvancedGreedy | 5 | 4280.39 | 1504.60 | 2775.80 | 0.692206 | 5 blockers |
| IMin SandIMIN BEST | 5 | 4368.27 | 1160.97 | 3207.31 | 24.0523 | best among LB/UB/OR |
| IMin SandIMIN LB | 5 | 4368.27 | 1167.51 | 3200.76 | 24.0523 | lower-bound candidate |
| IMin SandIMIN UB | 5 | 4368.27 | 1318.76 | 3049.52 | 24.0523 | upper-bound candidate |
| IMin SandIMIN OR | 5 | 4368.27 | 1924.55 | 2443.72 | 24.0523 | degree heuristic |
| AdvancedGreedy | 100 | 4280.39 | 105.856 | 4174.54 | 8.87024 | 100 blockers |
| IMin SandIMIN BEST | 100 | 4363.54 | 117.197 | 4246.34 | 55.6666 | best among LB/UB/OR |
| IMin SandIMIN LB | 100 | 4363.54 | 175.537 | 4188.00 | 55.6666 | lower-bound candidate |
| IMin SandIMIN UB | 100 | 4363.54 | 518.99 | 3844.55 | 55.6666 | upper-bound candidate |
| IMin SandIMIN OR | 100 | 4363.54 | 117.181 | 4246.36 | 55.6666 | degree heuristic |

## Approximation-ratio check

With `epsilon = 0.2`, `(1 - 1/e - epsilon) = 0.4321205588`.

The requested comparison is:

| Method | Formula | k=5 | k=100 | Status |
|---|---|---:|---:|---|
| SandIMIN-upper bound `DU(.)` | `D(BU) / DU(BU) * (1 - 1/e - epsilon)` | N/A | N/A | `DU(BU)` is not emitted by the current code/results file. |
| Proposed `U(.)` | `D(BU) / U(BU) * (1 - 1/e - epsilon)` | N/A | N/A | The repo does not contain the proposed `U(.)` / LaF path sampling implementation yet. |

Available SandIMIN internal upper-ratio stopping values observed in console:

| k | Last printed SandIMIN upper ratio |
|---:|---:|
| 5 | 0.490357 |
| 100 | 0.493599 |

These internal values are produced by the OPIM-C stopping test in SandIMIN and are not the same as the two formulas above.

## Raw files

| File | Description |
|---|---|
| `results/ag_smoke.tsv` | Raw AdvancedGreedy output rows |
| `results/imin_p2p_k5_raw.tsv` | Raw IMin result rows for `k=5` |
| `results/imin_p2p_k100_raw.tsv` | Raw IMin result rows for `k=100` |
| `results/p2p-Gnutella31_seed_node_10.txt` | Seed file shared by AdvancedGreedy |

