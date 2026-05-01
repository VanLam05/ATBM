# Influence Minimization via Vertex Blocking

Implementation of algorithms for the **Influence Minimization (IMIN)** problem, based on the paper:
> *"Minimizing the Influence of Misinformation via Vertex Blocking"* by Jiadong Xie et al., 2023.

## Algorithms

| Algorithm | Description | Code |
|---|---|---|
| **AdvancedGreedy (AG)** | Uses Monte Carlo sampling + dominator tree to efficiently compute spread decrease | `AdvancedGreedy/advanced_greedy.cpp` |
| **GreedyReplace (GR)** | Greedily selects out-neighbors, then iteratively replaces with better blockers | `AdvancedGreedy/advanced_greedy.cpp` |
| **IMin (SandIMIN)** | Sandwich approximation strategy using RR-sets | `IMin/SandIMIN_code/` |

## Build

```bash
# Build all
bash run_experiments.sh small   # compiles + quick test
bash run_experiments.sh full    # compiles + full experiment

# Or compile individually:
cd AdvancedGreedy && g++ -O3 -std=c++17 -o ag advanced_greedy.cpp
cd AdvancedGreedy && g++ -O3 -std=c++17 -o experiment experiment.cpp
cd IMin && g++ -O3 -std=c++17 -o prepare_dataset prepare_dataset.cpp
cd IMin/SandIMIN_code && g++ -O3 -o IMIN Sandwich.cpp sfmt/SFMT.c
```

## Usage

### AdvancedGreedy / GreedyReplace

```bash
./ag -dataset <path> -k <budget> -seedNum <|S|> -theta <#samples> -algo <AG|GR|BOTH>
# Example:
./ag -dataset ../../datasets/p2p-Gnutella31.txt -k 100 -seedNum 10 -theta 100 -algo BOTH
```

Parameters:
- `-dataset`: Path to dataset file (format: `n m\n directed|undirected\n u v ...`)
- `-k`: Blocking budget (default: 100)
- `-seedNum`: Seed set size |S| (default: 10)
- `-theta`: Number of sampled graphs for DecreaseESComputation (default: 100)
- `-mc`: Monte Carlo rounds for spread estimation (default: 10000)
- `-algo`: Algorithm to run: `AG`, `GR`, or `BOTH`
- `-timeLimit`: Maximum time in seconds (default: 100000)

### Experiment Runner

```bash
./experiment -datadir <datasets_dir> -theta 100 -mc 10000 -log results.txt
```

Runs experiments with:
- k ∈ {100, 200, 300, 400, 500} with fixed |S|=10
- |S| ∈ {10, 20, 30, 40, 50} with fixed k=100
- Estimates runtime before each run; skips if > 1 day

### IMin (SandIMIN)

First prepare the dataset:
```bash
cd IMin
./prepare_dataset -input ../../datasets/p2p-Gnutella31.txt -output SandIMIN_code/dataset/p2p-Gnutella31 -seedNum 50
```

Then run IMin:
```bash
cd IMin/SandIMIN_code
./IMIN -dataset dataset/p2p-Gnutella31 -k 100 -rumorNum 10 -algo SandIMIN -epsilon 0.2 -gamma 0.1 -beta 0.1
```

## Dataset Format

```
n m
directed|undirected
u1 v1
u2 v2
...
```

Propagation model: Weighted Cascade (WC) with p(u,v) = 1/|N_in(v)|

## Datasets

| Dataset | Nodes | Edges | Type |
|---|---|---|---|
| p2p-Gnutella31 | 62,586 | 147,892 | Directed |
| email-EuAll | 265,214 | 420,045 | Directed |
| com-dblp | 317,080 | 1,049,866 | Undirected |
| com-youtube | 1,134,890 | 2,987,624 | Undirected |

Seeds: Randomly selected from top-200 nodes by out-degree.
