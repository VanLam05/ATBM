/**
 * AdvancedGreedy (AG) and GreedyReplace (GR) algorithms for
 * Influence Minimization via Node Blocking.
 *
 * Reference:
 *   Jiadong Xie et al., "Minimizing the Influence of Misinformation
 *   via Vertex Blocking", 2023.
 *
 * IC model with Weighted Cascade (WC): p(u,v) = 1 / |N_in(v)|
 *
 * Dataset format:
 *   Line 1: n m
 *   Line 2: "directed" or "undirected"
 *   Lines 3..m+2: u v
 *
 * Compile:
 *   g++ -O3 -std=c++17 -o ag advanced_greedy.cpp
 *
 * Run:
 *   ./ag -dataset <path> -k <budget> -seedNum <|S|> -theta <#samples> -algo <AG|GR|BOTH>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cassert>
#include <unordered_set>
#include <functional>

using namespace std;
using namespace std::chrono;

// ============================================================
// Graph structure
// ============================================================
struct Graph
{
    int n, m;
    bool directed;
    // adjacency list: adj[u] = list of (v, prob)
    vector<vector<pair<int, double>>> adj;  // forward edges
    vector<vector<pair<int, double>>> radj; // reverse edges
    vector<int> in_degree;
    vector<int> out_degree;

    void read(const string &path)
    {
        ifstream fin(path);
        if (!fin.is_open())
        {
            cerr << "Cannot open file: " << path << endl;
            exit(1);
        }
        fin >> n >> m;
        string type;
        fin >> type;
        directed = (type == "directed");

        adj.resize(n);
        radj.resize(n);
        in_degree.assign(n, 0);
        out_degree.assign(n, 0);

        vector<pair<int, int>> edges;
        for (int i = 0; i < m; i++)
        {
            int u, v;
            fin >> u >> v;
            if (u < 0 || u >= n || v < 0 || v >= n)
                continue;
            if (u == v)
                continue;
            edges.push_back({u, v});
        }
        fin.close();

        // If undirected, add both directions
        if (!directed)
        {
            vector<pair<int, int>> undirected_edges;
            for (auto &[u, v] : edges)
            {
                undirected_edges.push_back({u, v});
                undirected_edges.push_back({v, u});
            }
            edges = undirected_edges;
        }

        // Count in-degrees first
        for (auto &[u, v] : edges)
        {
            in_degree[v]++;
            out_degree[u]++;
        }

        // WC model: p(u,v) = 1 / |N_in(v)|
        for (auto &[u, v] : edges)
        {
            double p = 1.0 / in_degree[v];
            adj[u].push_back({v, p});
            radj[v].push_back({u, p});
        }

        // Update actual m
        m = (int)edges.size();
    }
};

// ============================================================
// Seed set generation
// ============================================================
vector<int> generateSeedSet(const Graph &g, int seedNum, mt19937 &rng)
{
    // Find top-200 nodes by out-degree
    vector<int> indices(g.n);
    iota(indices.begin(), indices.end(), 0);
    partial_sort(indices.begin(),
                 indices.begin() + min(200, g.n),
                 indices.end(),
                 [&](int a, int b)
                 { return g.out_degree[a] > g.out_degree[b]; });

    int topK = min(200, g.n);
    vector<int> top200(indices.begin(), indices.begin() + topK);

    // Randomly choose seedNum from top-200
    shuffle(top200.begin(), top200.end(), rng);
    int actualSeedNum = min(seedNum, topK);
    vector<int> seeds(top200.begin(), top200.begin() + actualSeedNum);
    sort(seeds.begin(), seeds.end());
    return seeds;
}

// ============================================================
// Unified seed: merge multiple seeds into a virtual seed node
// For presentation, we add a virtual node n as the unified seed.
// ============================================================
struct UnifiedGraph
{
    int n; // original n + 1 (virtual seed)
    int virtualSeed;
    vector<vector<pair<int, double>>> adj;
    vector<vector<pair<int, double>>> radj;

    void build(const Graph &g, const vector<int> &seeds)
    {
        n = g.n + 1;
        virtualSeed = g.n;
        adj.resize(n);
        radj.resize(n);

        unordered_set<int> seedSet(seeds.begin(), seeds.end());

        // Copy original edges (skip edges from seeds, as they become edges from virtual seed)
        for (int u = 0; u < g.n; u++)
        {
            for (auto &[v, p] : g.adj[u])
            {
                if (seedSet.count(v))
                    continue; // skip edges into seed nodes
                if (seedSet.count(u))
                {
                    // edge from seed node -> becomes edge from virtual seed
                    // will be handled below with probability merging
                }
                else
                {
                    adj[u].push_back({v, p});
                    radj[v].push_back({u, p});
                }
            }
        }

        // For each non-seed node v that has edges from seed nodes,
        // merge them: p(s', v) = 1 - product(1 - pi)
        for (int v = 0; v < g.n; v++)
        {
            if (seedSet.count(v))
                continue;
            double prob_not_activated = 1.0;
            bool hasEdgeFromSeed = false;
            for (auto &[u, p] : g.radj[v])
            {
                if (seedSet.count(u))
                {
                    prob_not_activated *= (1.0 - p);
                    hasEdgeFromSeed = true;
                }
            }
            if (hasEdgeFromSeed)
            {
                double mergedProb = 1.0 - prob_not_activated;
                if (mergedProb > 0)
                {
                    adj[virtualSeed].push_back({v, mergedProb});
                    radj[v].push_back({virtualSeed, mergedProb});
                }
            }
        }
    }
};

// ============================================================
// Sampled graph generation
// ============================================================
struct SampledGraph
{
    int n;
    vector<vector<int>> children; // forward adjacency (no probs, just edges that survived)

    void generate(const UnifiedGraph &ug, mt19937 &rng, const vector<bool> &blocked)
    {
        n = ug.n;
        children.assign(n, vector<int>());
        uniform_real_distribution<double> dist(0.0, 1.0);
        for (int u = 0; u < n; u++)
        {
            if (blocked[u])
                continue;
            for (auto &[v, p] : ug.adj[u])
            {
                if (blocked[v])
                    continue;
                if (dist(rng) < p)
                {
                    children[u].push_back(v);
                }
            }
        }
    }
};

// ============================================================
// Dominator Tree (Lengauer-Tarjan Algorithm)
// ============================================================
class DominatorTree
{
public:
    int n;
    int root;
    vector<vector<int>> adj;     // forward edges of sampled graph
    vector<vector<int>> radj_lt; // reverse edges for LT
    vector<int> dfn;             // DFS order number
    vector<int> rev;             // rev[dfn] = node
    vector<int> parent;          // DFS tree parent
    vector<int> semi;            // semidominator
    vector<int> idom;            // immediate dominator
    vector<int> best;            // for path compression
    vector<int> ancestor;        // for union-find
    vector<vector<int>> bucket;
    int dfs_counter;

    vector<int> subtree_size;

    void build(const SampledGraph &sg, int root_node)
    {
        n = sg.n;
        root = root_node;
        adj.assign(n, vector<int>());
        radj_lt.assign(n, vector<int>());
        dfn.assign(n, 0);
        rev.assign(n + 1, -1);
        parent.assign(n, -1);
        semi.assign(n, -1);
        idom.assign(n, -1);
        best.assign(n, -1);
        ancestor.assign(n, -1);
        bucket.assign(n, vector<int>());
        dfs_counter = 0;

        // Build adjacency and reverse adjacency
        for (int u = 0; u < n; u++)
        {
            for (int v : sg.children[u])
            {
                adj[u].push_back(v);
                radj_lt[v].push_back(u);
            }
        }

        // DFS
        // Use iterative DFS to avoid stack overflow on large graphs
        dfs_iterative(root);

        if (dfs_counter == 0)
            return;

        // Initialize semi and best
        for (int i = 0; i < n; i++)
        {
            semi[i] = i;
            best[i] = i;
        }

        // Process vertices in reverse DFS order
        for (int i = dfs_counter; i >= 2; i--)
        {
            int w = rev[i];

            // Step 2: Compute semidominator
            for (int v : radj_lt[w])
            {
                if (dfn[v] == 0 && v != root)
                    continue; // not visited
                int u = eval(v);
                if (dfn[semi[u]] < dfn[semi[w]])
                {
                    semi[w] = semi[u];
                }
            }

            bucket[semi[w]].push_back(w);
            link(parent[w], w);

            // Step 3: Implicitly compute idom
            for (int v : bucket[parent[w]])
            {
                int u = eval(v);
                idom[v] = (semi[u] == semi[v]) ? parent[w] : u;
            }
            bucket[parent[w]].clear();
        }

        // Step 4: Explicitly compute idom
        for (int i = 2; i <= dfs_counter; i++)
        {
            int w = rev[i];
            if (idom[w] != semi[w])
            {
                idom[w] = idom[idom[w]];
            }
        }
        idom[root] = root;

        // Compute subtree sizes
        computeSubtreeSizes();
    }

    void dfs_iterative(int start)
    {
        struct Frame
        {
            int node;
            int child_idx;
        };
        vector<Frame> stack;
        stack.push_back({start, 0});
        dfn[start] = ++dfs_counter;
        rev[dfs_counter] = start;

        while (!stack.empty())
        {
            Frame &f = stack.back();
            if (f.child_idx < (int)adj[f.node].size())
            {
                int v = adj[f.node][f.child_idx];
                f.child_idx++;
                if (dfn[v] == 0)
                {
                    parent[v] = f.node;
                    dfn[v] = ++dfs_counter;
                    rev[dfs_counter] = v;
                    stack.push_back({v, 0});
                }
            }
            else
            {
                stack.pop_back();
            }
        }
    }

    void link(int v, int w)
    {
        ancestor[w] = v;
    }

    int eval(int v)
    {
        if (ancestor[v] == -1)
            return v;
        compress(v);
        return best[v];
    }

    void compress(int v)
    {
        if (ancestor[v] == -1)
            return;
        if (ancestor[ancestor[v]] == -1)
            return;
        compress(ancestor[v]);
        if (dfn[semi[best[ancestor[v]]]] < dfn[semi[best[v]]])
        {
            best[v] = best[ancestor[v]];
        }
        ancestor[v] = ancestor[ancestor[v]];
    }

    void computeSubtreeSizes()
    {
        subtree_size.assign(n, 0);
        // Build dominator tree edges
        vector<vector<int>> dom_children(n);
        for (int i = 1; i <= dfs_counter; i++)
        {
            int w = rev[i];
            if (w != root && idom[w] != -1 && idom[w] != w)
            {
                dom_children[idom[w]].push_back(w);
            }
        }

        // Iterative post-order traversal
        vector<int> order;
        {
            vector<int> stk;
            stk.push_back(root);
            while (!stk.empty())
            {
                int u = stk.back();
                stk.pop_back();
                order.push_back(u);
                for (int c : dom_children[u])
                {
                    stk.push_back(c);
                }
            }
        }
        // Process in reverse (post-order)
        for (int i = (int)order.size() - 1; i >= 0; i--)
        {
            int u = order[i];
            subtree_size[u] = 1;
            for (int c : dom_children[u])
            {
                subtree_size[u] += subtree_size[c];
            }
        }

        // Only count nodes that were visited in DFS
        for (int i = 0; i < n; i++)
        {
            if (dfn[i] == 0 && i != root)
            {
                subtree_size[i] = 0;
            }
        }
    }
};

// ============================================================
// DecreaseESComputation (Algorithm 2)
// ============================================================
vector<double> DecreaseESComputation(
    const UnifiedGraph &ug,
    int theta,
    const vector<bool> &blocked,
    mt19937 &rng)
{
    int n = ug.n;
    vector<double> delta(n, 0.0);

    for (int t = 0; t < theta; t++)
    {
        SampledGraph sg;
        sg.generate(ug, rng, blocked);

        DominatorTree dt;
        dt.build(sg, ug.virtualSeed);

        for (int u = 0; u < n; u++)
        {
            if (u == ug.virtualSeed)
                continue;
            if (blocked[u])
                continue;
            delta[u] += (double)dt.subtree_size[u] / theta;
        }
    }

    return delta;
}

// ============================================================
// Monte Carlo estimation of influence spread
// ============================================================
double MC_estimate(const UnifiedGraph &ug, const vector<bool> &blocked,
                   int mc_rounds, mt19937 &rng)
{
    double total = 0.0;
    uniform_real_distribution<double> dist(0.0, 1.0);

    for (int t = 0; t < mc_rounds; t++)
    {
        // BFS from virtual seed
        vector<bool> visited(ug.n, false);
        visited[ug.virtualSeed] = true;
        vector<int> queue;
        queue.push_back(ug.virtualSeed);
        int spread = 0; // don't count virtual seed

        int front = 0;
        while (front < (int)queue.size())
        {
            int u = queue[front++];
            for (auto &[v, p] : ug.adj[u])
            {
                if (visited[v] || blocked[v])
                    continue;
                if (dist(rng) < p)
                {
                    visited[v] = true;
                    queue.push_back(v);
                    spread++;
                }
            }
        }
        total += spread;
    }
    return total / mc_rounds;
}

// ============================================================
// AdvancedGreedy Algorithm (Algorithm 3)
// ============================================================
struct AlgoResult
{
    vector<int> blockers;
    double spread_before; // E(S, G) with no blockers
    double spread_after;  // E(S, G[V\B])
    double saved_nodes;   // spread_before - spread_after
    double time_seconds;
};

AlgoResult AdvancedGreedy(
    const UnifiedGraph &ug,
    const vector<int> &seeds,
    int budget,
    int theta,
    int mc_rounds,
    mt19937 &rng,
    double time_limit = 1e5)
{
    AlgoResult result;
    auto startTime = high_resolution_clock::now();

    int n = ug.n;
    vector<bool> blocked(n, false);
    // Block seed nodes (they are already merged into virtual seed)
    for (int s : seeds)
        blocked[s] = true;

    result.spread_before = MC_estimate(ug, blocked, mc_rounds, rng);
    cout << "  [AG] Initial spread estimate: " << result.spread_before << endl;

    result.blockers.clear();

    for (int i = 0; i < budget; i++)
    {
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        if (elapsed > time_limit)
        {
            cout << "  [AG] Time limit exceeded (" << elapsed << "s > " << time_limit << "s). Stopping at k=" << i << endl;
            break;
        }

        cout << "  [AG] Selecting blocker " << (i + 1) << "/" << budget << "..." << flush;

        vector<double> delta = DecreaseESComputation(ug, theta, blocked, rng);

        int bestNode = -1;
        double bestDelta = -1;
        for (int u = 0; u < n; u++)
        {
            if (u == ug.virtualSeed)
                continue;
            if (blocked[u])
                continue;
            if (delta[u] > bestDelta)
            {
                bestDelta = delta[u];
                bestNode = u;
            }
        }

        if (bestNode == -1)
            break;

        blocked[bestNode] = true;
        result.blockers.push_back(bestNode);

        now = high_resolution_clock::now();
        elapsed = duration_cast<duration<double>>(now - startTime).count();
        cout << " node=" << bestNode << " delta=" << bestDelta << " time=" << elapsed << "s" << endl;
    }

    result.spread_after = MC_estimate(ug, blocked, mc_rounds, rng);
    result.saved_nodes = result.spread_before - result.spread_after;

    auto endTime = high_resolution_clock::now();
    result.time_seconds = duration_cast<duration<double>>(endTime - startTime).count();

    return result;
}

// ============================================================
// GreedyReplace Algorithm (Algorithm 4)
// ============================================================
AlgoResult GreedyReplace(
    const UnifiedGraph &ug,
    const vector<int> &seeds,
    int budget,
    int theta,
    int mc_rounds,
    mt19937 &rng,
    double time_limit = 1e5)
{
    AlgoResult result;
    auto startTime = high_resolution_clock::now();

    int n = ug.n;
    vector<bool> blocked(n, false);
    unordered_set<int> seedSet(seeds.begin(), seeds.end());
    for (int s : seeds)
        blocked[s] = true;

    result.spread_before = MC_estimate(ug, blocked, mc_rounds, rng);
    cout << "  [GR] Initial spread estimate: " << result.spread_before << endl;

    // Phase 1: Collect out-neighbors of seed set (candidate blockers CB)
    unordered_set<int> CB_set;
    for (int s : seeds)
    {
        // For the original graph, we need out-neighbors of seeds
        // In the unified graph, out-neighbors of virtual seed are the CB
    }
    for (auto &[v, p] : ug.adj[ug.virtualSeed])
    {
        if (!seedSet.count(v))
        {
            CB_set.insert(v);
        }
    }
    vector<int> CB(CB_set.begin(), CB_set.end());

    // Phase 1: Greedily select min(|CB|, budget) out-neighbors
    vector<int> B; // blocker set (ordered by insertion)
    int phase1_count = min((int)CB.size(), budget);

    for (int i = 0; i < phase1_count; i++)
    {
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        if (elapsed > time_limit)
        {
            cout << "  [GR] Time limit exceeded in Phase 1. Stopping." << endl;
            break;
        }

        cout << "  [GR] Phase 1: Selecting out-neighbor blocker " << (i + 1) << "/" << phase1_count << "..." << flush;

        vector<double> delta = DecreaseESComputation(ug, theta, blocked, rng);

        int bestNode = -1;
        double bestDelta = -1;
        for (int u : CB)
        {
            if (blocked[u])
                continue;
            if (delta[u] > bestDelta)
            {
                bestDelta = delta[u];
                bestNode = u;
            }
        }

        if (bestNode == -1)
            break;

        blocked[bestNode] = true;
        B.push_back(bestNode);
        // Remove from CB
        CB.erase(remove(CB.begin(), CB.end(), bestNode), CB.end());

        now = high_resolution_clock::now();
        elapsed = duration_cast<duration<double>>(now - startTime).count();
        cout << " node=" << bestNode << " delta=" << bestDelta << " time=" << elapsed << "s" << endl;
    }

    // Phase 2: Replace blockers in reverse insertion order
    cout << "  [GR] Phase 2: Replacing blockers..." << endl;
    for (int idx = (int)B.size() - 1; idx >= 0; idx--)
    {
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        if (elapsed > time_limit)
        {
            cout << "  [GR] Time limit exceeded in Phase 2. Stopping." << endl;
            break;
        }

        int u = B[idx];
        // Remove u from blocker set temporarily
        blocked[u] = false;

        cout << "  [GR] Phase 2: Trying to replace blocker " << u << "..." << flush;

        vector<double> delta = DecreaseESComputation(ug, theta, blocked, rng);

        int bestNode = -1;
        double bestDelta = -1;
        for (int v = 0; v < n; v++)
        {
            if (v == ug.virtualSeed)
                continue;
            if (blocked[v])
                continue;
            if (seedSet.count(v))
                continue;
            if (delta[v] > bestDelta)
            {
                bestDelta = delta[v];
                bestNode = v;
            }
        }

        if (bestNode == -1)
        {
            blocked[u] = true; // put it back
        }
        else
        {
            blocked[bestNode] = true;
            B[idx] = bestNode;

            now = high_resolution_clock::now();
            elapsed = duration_cast<duration<double>>(now - startTime).count();

            if (bestNode == u)
            {
                cout << " kept node=" << u << " time=" << elapsed << "s (early terminate)" << endl;
                break;
            }
            else
            {
                cout << " replaced with node=" << bestNode << " delta=" << bestDelta << " time=" << elapsed << "s" << endl;
            }
        }
    }

    result.blockers = B;
    result.spread_after = MC_estimate(ug, blocked, mc_rounds, rng);
    result.saved_nodes = result.spread_before - result.spread_after;

    auto endTime = high_resolution_clock::now();
    result.time_seconds = duration_cast<duration<double>>(endTime - startTime).count();

    return result;
}

// ============================================================
// Time estimation on small subgraph
// ============================================================
double estimateRuntime(const UnifiedGraph &ug, int budget, int theta,
                       mt19937 &rng, int sample_iters = 3)
{
    vector<bool> blocked(ug.n, false);
    // Block seed nodes
    blocked[ug.virtualSeed] = false; // virtual seed is never blocked

    auto start = high_resolution_clock::now();
    for (int i = 0; i < sample_iters; i++)
    {
        DecreaseESComputation(ug, theta, blocked, rng);
    }
    auto end = high_resolution_clock::now();
    double per_iter = duration_cast<duration<double>>(end - start).count() / sample_iters;

    // AG: budget iterations of DecreaseESComputation
    double ag_estimate = per_iter * budget;
    // GR: ~2*budget iterations
    double gr_estimate = per_iter * budget * 2;

    return max(ag_estimate, gr_estimate);
}

// ============================================================
// Load or generate seed set
// ============================================================
vector<int> loadOrGenerateSeeds(const string &seedFile, const Graph &g,
                                int seedNum, mt19937 &rng)
{
    ifstream fin(seedFile);
    if (fin.is_open())
    {
        vector<int> seeds;
        int node;
        while (fin >> node)
        {
            if (node >= 0 && node < g.n)
            {
                seeds.push_back(node);
            }
        }
        fin.close();
        if (!seeds.empty())
        {
            cout << "Loaded " << seeds.size() << " seeds from " << seedFile << endl;
            return seeds;
        }
    }

    // Generate
    vector<int> seeds = generateSeedSet(g, seedNum, rng);

    // Save
    ofstream fout(seedFile);
    for (int s : seeds)
        fout << s << "\n";
    fout.close();
    cout << "Generated " << seeds.size() << " seeds, saved to " << seedFile << endl;

    return seeds;
}

// ============================================================
// Main
// ============================================================
int main(int argc, char **argv)
{
    string dataset = "";
    int budget = 100;
    int seedNum = 10;
    int theta = 100;
    int mc_rounds = 10000;
    string algo = "BOTH";
    double time_limit = 1e5;
    string seedFile = "";
    string outputFile = "";

    for (int i = 1; i < argc; i++)
    {
        if (string(argv[i]) == "-dataset")
            dataset = argv[++i];
        else if (string(argv[i]) == "-k")
            budget = atoi(argv[++i]);
        else if (string(argv[i]) == "-seedNum")
            seedNum = atoi(argv[++i]);
        else if (string(argv[i]) == "-theta")
            theta = atoi(argv[++i]);
        else if (string(argv[i]) == "-mc")
            mc_rounds = atoi(argv[++i]);
        else if (string(argv[i]) == "-algo")
            algo = argv[++i];
        else if (string(argv[i]) == "-timeLimit")
            time_limit = stod(argv[++i]);
        else if (string(argv[i]) == "-seedFile")
            seedFile = argv[++i];
        else if (string(argv[i]) == "-output")
            outputFile = argv[++i];
    }

    if (dataset.empty())
    {
        cerr << "Usage: ./ag -dataset <path> [-k budget] [-seedNum |S|] [-theta #samples] [-algo AG|GR|BOTH] [-timeLimit seconds] [-seedFile path] [-output path]" << endl;
        return 0;
    }

    cout << "=== Configuration ===" << endl;
    cout << "Dataset: " << dataset << endl;
    cout << "Budget k: " << budget << endl;
    cout << "Seed size |S|: " << seedNum << endl;
    cout << "Theta (sampled graphs): " << theta << endl;
    cout << "MC rounds: " << mc_rounds << endl;
    cout << "Algorithm: " << algo << endl;
    cout << "Time limit: " << time_limit << "s" << endl;

    // Read graph
    Graph g;
    cout << "\nReading graph..." << endl;
    g.read(dataset);
    cout << "Graph: n=" << g.n << " m=" << g.m << " directed=" << g.directed << endl;

    // Generate/load seeds
    mt19937 rng(42);
    if (seedFile.empty())
    {
        // derive seed file from dataset path
        size_t lastSlash = dataset.find_last_of("/\\");
        string baseName = (lastSlash != string::npos) ? dataset.substr(lastSlash + 1) : dataset;
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != string::npos)
            baseName = baseName.substr(0, dotPos);
        seedFile = baseName + "_seed_" + to_string(seedNum) + ".txt";
        // Put seed file next to dataset
        if (lastSlash != string::npos)
        {
            seedFile = dataset.substr(0, lastSlash + 1) + seedFile;
        }
    }
    vector<int> seeds = loadOrGenerateSeeds(seedFile, g, seedNum, rng);
    cout << "Seeds: ";
    for (int s : seeds)
        cout << s << " ";
    cout << endl;

    // Build unified graph
    UnifiedGraph ug;
    ug.build(g, seeds);
    cout << "Unified graph: n=" << ug.n << " virtualSeed=" << ug.virtualSeed << endl;

    // Open output file
    ofstream ofs;
    if (!outputFile.empty())
    {
        ofs.open(outputFile, ios::app);
    }

    // Run algorithms
    if (algo == "AG" || algo == "BOTH")
    {
        cout << "\n=== Running AdvancedGreedy ===" << endl;
        mt19937 rng_ag(123);
        AlgoResult res = AdvancedGreedy(ug, seeds, budget, theta, mc_rounds, rng_ag, time_limit);
        cout << "\n--- AdvancedGreedy Results ---" << endl;
        cout << "Spread before blocking: " << res.spread_before << endl;
        cout << "Spread after blocking: " << res.spread_after << endl;
        cout << "Saved nodes: " << res.saved_nodes << endl;
        cout << "Time: " << res.time_seconds << "s" << endl;
        cout << "Blockers selected: " << res.blockers.size() << endl;

        if (ofs.is_open())
        {
            ofs << "AG\t" << dataset << "\tk=" << budget << "\t|S|=" << seedNum
                << "\ttheta=" << theta
                << "\tspread_before=" << res.spread_before
                << "\tspread_after=" << res.spread_after
                << "\tsaved=" << res.saved_nodes
                << "\ttime=" << res.time_seconds << "s"
                << "\tblockers=" << res.blockers.size() << endl;
        }
    }

    if (algo == "GR" || algo == "BOTH")
    {
        cout << "\n=== Running GreedyReplace ===" << endl;
        mt19937 rng_gr(456);
        AlgoResult res = GreedyReplace(ug, seeds, budget, theta, mc_rounds, rng_gr, time_limit);
        cout << "\n--- GreedyReplace Results ---" << endl;
        cout << "Spread before blocking: " << res.spread_before << endl;
        cout << "Spread after blocking: " << res.spread_after << endl;
        cout << "Saved nodes: " << res.saved_nodes << endl;
        cout << "Time: " << res.time_seconds << "s" << endl;
        cout << "Blockers selected: " << res.blockers.size() << endl;

        if (ofs.is_open())
        {
            ofs << "GR\t" << dataset << "\tk=" << budget << "\t|S|=" << seedNum
                << "\ttheta=" << theta
                << "\tspread_before=" << res.spread_before
                << "\tspread_after=" << res.spread_after
                << "\tsaved=" << res.saved_nodes
                << "\ttime=" << res.time_seconds << "s"
                << "\tblockers=" << res.blockers.size() << endl;
        }
    }

    if (ofs.is_open())
        ofs.close();

    return 0;
}
