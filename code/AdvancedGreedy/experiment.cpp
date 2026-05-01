/**
 * Experiment runner for AdvancedGreedy (AG), GreedyReplace (GR) algorithms.
 *
 * This program:
 * 1. Runs a small estimation first on a subgraph to predict runtime
 * 2. If estimated runtime > 1 day (86400s), logs and skips
 * 3. Otherwise runs the full experiment
 *
 * Experiments from requires.pdf:
 * - Default |S|=10, k=100
 * - Vary k in {100, 200, 300, 400, 500} with fixed |S|=10
 * - Vary |S| in {10, 20, 30, 40, 50} with fixed k=100
 * - Time limit per run: 1e5 seconds
 *
 * Compile:
 *   g++ -O3 -std=c++17 -o experiment experiment.cpp
 *
 * Run:
 *   ./experiment -datadir <datasets_dir> [-theta 100] [-mc 10000]
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
#include <filesystem>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

// ============================================================
// Graph structure
// ============================================================
struct Graph {
    int n, m;
    bool directed;
    vector<vector<pair<int,double>>> adj;
    vector<vector<pair<int,double>>> radj;
    vector<int> in_degree;
    vector<int> out_degree;

    void read(const string& path) {
        ifstream fin(path);
        if (!fin.is_open()) {
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

        vector<pair<int,int>> edges;
        for (int i = 0; i < m; i++) {
            int u, v;
            fin >> u >> v;
            if (u < 0 || u >= n || v < 0 || v >= n) continue;
            if (u == v) continue;
            edges.push_back({u, v});
        }
        fin.close();

        if (!directed) {
            vector<pair<int,int>> undirected_edges;
            for (auto& [u, v] : edges) {
                undirected_edges.push_back({u, v});
                undirected_edges.push_back({v, u});
            }
            edges = undirected_edges;
        }

        for (auto& [u, v] : edges) {
            in_degree[v]++;
            out_degree[u]++;
        }

        for (auto& [u, v] : edges) {
            double p = 1.0 / in_degree[v];
            adj[u].push_back({v, p});
            radj[v].push_back({u, p});
        }

        m = (int)edges.size();
    }
};

// ============================================================
// Seed set generation
// ============================================================
vector<int> generateSeedSet(const Graph& g, int seedNum, mt19937& rng) {
    vector<int> indices(g.n);
    iota(indices.begin(), indices.end(), 0);
    partial_sort(indices.begin(),
                 indices.begin() + min(200, g.n),
                 indices.end(),
                 [&](int a, int b) { return g.out_degree[a] > g.out_degree[b]; });

    int topK = min(200, g.n);
    vector<int> top200(indices.begin(), indices.begin() + topK);
    shuffle(top200.begin(), top200.end(), rng);
    int actualSeedNum = min(seedNum, topK);
    vector<int> seeds(top200.begin(), top200.begin() + actualSeedNum);
    sort(seeds.begin(), seeds.end());
    return seeds;
}

// ============================================================
// Unified graph
// ============================================================
struct UnifiedGraph {
    int n;
    int virtualSeed;
    vector<vector<pair<int,double>>> adj;
    vector<vector<pair<int,double>>> radj;

    void build(const Graph& g, const vector<int>& seeds) {
        n = g.n + 1;
        virtualSeed = g.n;
        adj.resize(n);
        radj.resize(n);

        unordered_set<int> seedSet(seeds.begin(), seeds.end());

        for (int u = 0; u < g.n; u++) {
            for (auto& [v, p] : g.adj[u]) {
                if (seedSet.count(v)) continue;
                if (!seedSet.count(u)) {
                    adj[u].push_back({v, p});
                    radj[v].push_back({u, p});
                }
            }
        }

        for (int v = 0; v < g.n; v++) {
            if (seedSet.count(v)) continue;
            double prob_not_activated = 1.0;
            bool hasEdgeFromSeed = false;
            for (auto& [u, p] : g.radj[v]) {
                if (seedSet.count(u)) {
                    prob_not_activated *= (1.0 - p);
                    hasEdgeFromSeed = true;
                }
            }
            if (hasEdgeFromSeed) {
                double mergedProb = 1.0 - prob_not_activated;
                if (mergedProb > 0) {
                    adj[virtualSeed].push_back({v, mergedProb});
                    radj[v].push_back({virtualSeed, mergedProb});
                }
            }
        }
    }
};

// ============================================================
// Sampled graph
// ============================================================
struct SampledGraph {
    int n;
    vector<vector<int>> children;

    void generate(const UnifiedGraph& ug, mt19937& rng, const vector<bool>& blocked) {
        n = ug.n;
        children.assign(n, vector<int>());
        uniform_real_distribution<double> dist(0.0, 1.0);
        for (int u = 0; u < n; u++) {
            if (blocked[u]) continue;
            for (auto& [v, p] : ug.adj[u]) {
                if (blocked[v]) continue;
                if (dist(rng) < p) {
                    children[u].push_back(v);
                }
            }
        }
    }
};

// ============================================================
// Dominator Tree (Lengauer-Tarjan)
// ============================================================
class DominatorTree {
public:
    int n, root;
    vector<vector<int>> adj;
    vector<vector<int>> radj_lt;
    vector<int> dfn, rev, parent_node, semi, idom, best, ancestor;
    vector<vector<int>> bucket;
    int dfs_counter;
    vector<int> subtree_size;

    void build(const SampledGraph& sg, int root_node) {
        n = sg.n;
        root = root_node;
        adj.assign(n, {});
        radj_lt.assign(n, {});
        dfn.assign(n, 0);
        rev.assign(n + 1, -1);
        parent_node.assign(n, -1);
        semi.assign(n, -1);
        idom.assign(n, -1);
        best.assign(n, -1);
        ancestor.assign(n, -1);
        bucket.assign(n, {});
        dfs_counter = 0;

        for (int u = 0; u < n; u++)
            for (int v : sg.children[u]) {
                adj[u].push_back(v);
                radj_lt[v].push_back(u);
            }

        dfs_iterative(root);
        if (dfs_counter == 0) return;

        for (int i = 0; i < n; i++) { semi[i] = i; best[i] = i; }

        for (int i = dfs_counter; i >= 2; i--) {
            int w = rev[i];
            for (int v : radj_lt[w]) {
                if (dfn[v] == 0 && v != root) continue;
                int u = eval(v);
                if (dfn[semi[u]] < dfn[semi[w]]) semi[w] = semi[u];
            }
            bucket[semi[w]].push_back(w);
            link(parent_node[w], w);
            for (int v : bucket[parent_node[w]]) {
                int u = eval(v);
                idom[v] = (semi[u] == semi[v]) ? parent_node[w] : u;
            }
            bucket[parent_node[w]].clear();
        }

        for (int i = 2; i <= dfs_counter; i++) {
            int w = rev[i];
            if (idom[w] != semi[w]) idom[w] = idom[idom[w]];
        }
        idom[root] = root;
        computeSubtreeSizes();
    }

    void dfs_iterative(int start) {
        struct Frame { int node, child_idx; };
        vector<Frame> stack;
        stack.push_back({start, 0});
        dfn[start] = ++dfs_counter;
        rev[dfs_counter] = start;
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.child_idx < (int)adj[f.node].size()) {
                int v = adj[f.node][f.child_idx++];
                if (dfn[v] == 0) {
                    parent_node[v] = f.node;
                    dfn[v] = ++dfs_counter;
                    rev[dfs_counter] = v;
                    stack.push_back({v, 0});
                }
            } else stack.pop_back();
        }
    }
    void link(int v, int w) { ancestor[w] = v; }
    int eval(int v) {
        if (ancestor[v] == -1) return v;
        compress(v);
        return best[v];
    }
    void compress(int v) {
        if (ancestor[v] == -1) return;
        if (ancestor[ancestor[v]] == -1) return;
        compress(ancestor[v]);
        if (dfn[semi[best[ancestor[v]]]] < dfn[semi[best[v]]])
            best[v] = best[ancestor[v]];
        ancestor[v] = ancestor[ancestor[v]];
    }
    void computeSubtreeSizes() {
        subtree_size.assign(n, 0);
        vector<vector<int>> dom_children(n);
        for (int i = 1; i <= dfs_counter; i++) {
            int w = rev[i];
            if (w != root && idom[w] != -1 && idom[w] != w)
                dom_children[idom[w]].push_back(w);
        }
        vector<int> order;
        { vector<int> stk; stk.push_back(root);
          while (!stk.empty()) {
            int u = stk.back(); stk.pop_back();
            order.push_back(u);
            for (int c : dom_children[u]) stk.push_back(c);
          }
        }
        for (int i = (int)order.size()-1; i >= 0; i--) {
            int u = order[i];
            subtree_size[u] = 1;
            for (int c : dom_children[u]) subtree_size[u] += subtree_size[c];
        }
        for (int i = 0; i < n; i++)
            if (dfn[i] == 0 && i != root) subtree_size[i] = 0;
    }
};

// ============================================================
// DecreaseESComputation
// ============================================================
vector<double> DecreaseESComputation(
    const UnifiedGraph& ug, int theta,
    const vector<bool>& blocked, mt19937& rng)
{
    int n = ug.n;
    vector<double> delta(n, 0.0);
    for (int t = 0; t < theta; t++) {
        SampledGraph sg;
        sg.generate(ug, rng, blocked);
        DominatorTree dt;
        dt.build(sg, ug.virtualSeed);
        for (int u = 0; u < n; u++) {
            if (u == ug.virtualSeed || blocked[u]) continue;
            delta[u] += (double)dt.subtree_size[u] / theta;
        }
    }
    return delta;
}

// ============================================================
// MC estimation
// ============================================================
double MC_estimate(const UnifiedGraph& ug, const vector<bool>& blocked,
                   int mc_rounds, mt19937& rng) {
    double total = 0.0;
    uniform_real_distribution<double> dist(0.0, 1.0);
    for (int t = 0; t < mc_rounds; t++) {
        vector<bool> visited(ug.n, false);
        visited[ug.virtualSeed] = true;
        vector<int> queue;
        queue.push_back(ug.virtualSeed);
        int spread = 0;
        int front = 0;
        while (front < (int)queue.size()) {
            int u = queue[front++];
            for (auto& [v, p] : ug.adj[u]) {
                if (visited[v] || blocked[v]) continue;
                if (dist(rng) < p) {
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
// Algorithm result
// ============================================================
struct AlgoResult {
    vector<int> blockers;
    double spread_before;
    double spread_after;
    double saved_nodes;
    double time_seconds;
};

// ============================================================
// AdvancedGreedy
// ============================================================
AlgoResult AdvancedGreedy(
    const UnifiedGraph& ug, const vector<int>& seeds,
    int budget, int theta, int mc_rounds, mt19937& rng,
    double time_limit = 1e5)
{
    AlgoResult result;
    auto startTime = high_resolution_clock::now();
    int n = ug.n;
    vector<bool> blocked(n, false);
    for (int s : seeds) blocked[s] = true;

    result.spread_before = MC_estimate(ug, blocked, mc_rounds, rng);

    for (int i = 0; i < budget; i++) {
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        if (elapsed > time_limit) {
            cout << "  [AG] Time limit exceeded at k=" << i << " (" << elapsed << "s)" << endl;
            break;
        }
        if ((i+1) % 10 == 0 || i == 0)
            cout << "  [AG] k=" << (i+1) << "/" << budget << " elapsed=" << elapsed << "s" << endl;

        vector<double> delta = DecreaseESComputation(ug, theta, blocked, rng);
        int bestNode = -1;
        double bestDelta = -1;
        for (int u = 0; u < n; u++) {
            if (u == ug.virtualSeed || blocked[u]) continue;
            if (delta[u] > bestDelta) { bestDelta = delta[u]; bestNode = u; }
        }
        if (bestNode == -1) break;
        blocked[bestNode] = true;
        result.blockers.push_back(bestNode);
    }

    result.spread_after = MC_estimate(ug, blocked, mc_rounds, rng);
    result.saved_nodes = result.spread_before - result.spread_after;
    auto endTime = high_resolution_clock::now();
    result.time_seconds = duration_cast<duration<double>>(endTime - startTime).count();
    return result;
}

// ============================================================
// GreedyReplace
// ============================================================
AlgoResult GreedyReplace(
    const UnifiedGraph& ug, const vector<int>& seeds,
    int budget, int theta, int mc_rounds, mt19937& rng,
    double time_limit = 1e5)
{
    AlgoResult result;
    auto startTime = high_resolution_clock::now();
    int n = ug.n;
    vector<bool> blocked(n, false);
    unordered_set<int> seedSet(seeds.begin(), seeds.end());
    for (int s : seeds) blocked[s] = true;

    result.spread_before = MC_estimate(ug, blocked, mc_rounds, rng);

    // Phase 1: out-neighbors
    unordered_set<int> CB_set;
    for (auto& [v, p] : ug.adj[ug.virtualSeed]) {
        if (!seedSet.count(v)) CB_set.insert(v);
    }
    vector<int> CB(CB_set.begin(), CB_set.end());
    vector<int> B;
    int phase1_count = min((int)CB.size(), budget);

    for (int i = 0; i < phase1_count; i++) {
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        if (elapsed > time_limit) break;

        vector<double> delta = DecreaseESComputation(ug, theta, blocked, rng);
        int bestNode = -1; double bestDelta = -1;
        for (int u : CB) {
            if (blocked[u]) continue;
            if (delta[u] > bestDelta) { bestDelta = delta[u]; bestNode = u; }
        }
        if (bestNode == -1) break;
        blocked[bestNode] = true;
        B.push_back(bestNode);
        CB.erase(remove(CB.begin(), CB.end(), bestNode), CB.end());
    }

    // Phase 2: replace
    for (int idx = (int)B.size()-1; idx >= 0; idx--) {
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<duration<double>>(now - startTime).count();
        if (elapsed > time_limit) break;

        int u = B[idx];
        blocked[u] = false;
        vector<double> delta = DecreaseESComputation(ug, theta, blocked, rng);
        int bestNode = -1; double bestDelta = -1;
        for (int v = 0; v < n; v++) {
            if (v == ug.virtualSeed || blocked[v] || seedSet.count(v)) continue;
            if (delta[v] > bestDelta) { bestDelta = delta[v]; bestNode = v; }
        }
        if (bestNode == -1) { blocked[u] = true; }
        else {
            blocked[bestNode] = true;
            B[idx] = bestNode;
            if (bestNode == u) break;
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
// Estimate runtime for one iteration of DecreaseESComputation
// ============================================================
double estimateOneIteration(const UnifiedGraph& ug, int theta, mt19937& rng) {
    vector<bool> blocked(ug.n, false);
    auto start = high_resolution_clock::now();
    int test_iters = 3;
    for (int i = 0; i < test_iters; i++) {
        DecreaseESComputation(ug, theta, blocked, rng);
    }
    auto end = high_resolution_clock::now();
    return duration_cast<duration<double>>(end - start).count() / test_iters;
}

// ============================================================
// Load or generate seed set
// ============================================================
vector<int> loadOrGenerateSeeds(const string& seedFile, const Graph& g,
                                 int seedNum, mt19937& rng) {
    ifstream fin(seedFile);
    if (fin.is_open()) {
        vector<int> seeds;
        int node;
        while (fin >> node) {
            if (node >= 0 && node < g.n) seeds.push_back(node);
        }
        fin.close();
        if ((int)seeds.size() >= seedNum) {
            seeds.resize(seedNum);
            cout << "  Loaded " << seeds.size() << " seeds from " << seedFile << endl;
            return seeds;
        }
    }
    vector<int> seeds = generateSeedSet(g, seedNum, rng);
    ofstream fout(seedFile);
    for (int s : seeds) fout << s << "\n";
    fout.close();
    cout << "  Generated " << seeds.size() << " seeds -> " << seedFile << endl;
    return seeds;
}

// ============================================================
// Main experiment runner
// ============================================================
int main(int argc, char** argv) {
    string datadir = "";
    int theta = 100;
    int mc_rounds = 10000;
    double time_limit = 1e5; // 100,000 seconds per run
    double day_limit = 86400.0; // 1 day in seconds
    string logFile = "experiment_results.txt";

    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "-datadir") datadir = argv[++i];
        else if (string(argv[i]) == "-theta") theta = atoi(argv[++i]);
        else if (string(argv[i]) == "-mc") mc_rounds = atoi(argv[++i]);
        else if (string(argv[i]) == "-timeLimit") time_limit = stod(argv[++i]);
        else if (string(argv[i]) == "-log") logFile = argv[++i];
    }

    if (datadir.empty()) {
        cerr << "Usage: ./experiment -datadir <datasets_dir> [-theta 100] [-mc 10000] [-log results.txt]" << endl;
        return 1;
    }

    // Dataset files
    vector<string> datasets = {
        "p2p-Gnutella31.txt",
        "email-EuAll.txt",
        "com-dblp.ungraph.txt",
        "com-youtube.ungraph.txt"
    };

    // Experiment parameters
    vector<int> k_values = {100, 200, 300, 400, 500};
    vector<int> s_values = {10, 20, 30, 40, 50};
    int default_k = 100;
    int default_s = 10;

    ofstream logOut(logFile, ios::app);
    logOut << "=== Experiment started at " << time(nullptr) << " ===" << endl;
    logOut << "theta=" << theta << " mc=" << mc_rounds << " timeLimit=" << time_limit << endl;
    logOut << "Format: algo\tdataset\tk\t|S|\tspread_before\tspread_after\tsaved\ttime(s)\tstatus" << endl;
    logOut << "---" << endl;

    for (const string& dsName : datasets) {
        string dsPath = datadir + "/" + dsName;
        if (!fs::exists(dsPath)) {
            cout << "\n[SKIP] Dataset not found: " << dsPath << endl;
            logOut << "SKIP\t" << dsName << "\tFile not found" << endl;
            continue;
        }

        cout << "\n========================================" << endl;
        cout << "Dataset: " << dsName << endl;
        cout << "========================================" << endl;

        Graph g;
        g.read(dsPath);
        cout << "Graph loaded: n=" << g.n << " m=" << g.m << " directed=" << g.directed << endl;

        // Generate seed file path base
        string baseName = dsName;
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != string::npos) baseName = baseName.substr(0, dotPos);

        // Experiment 1: Vary k with fixed |S|=10
        cout << "\n--- Experiment 1: Vary k, |S|=" << default_s << " ---" << endl;
        {
            mt19937 rng_seed(42);
            string seedFile = datadir + "/" + baseName + "_seed_" + to_string(default_s) + ".txt";
            vector<int> seeds = loadOrGenerateSeeds(seedFile, g, default_s, rng_seed);

            UnifiedGraph ug;
            ug.build(g, seeds);

            // Estimate runtime
            cout << "  Estimating runtime..." << endl;
            mt19937 rng_est(789);
            double oneIter = estimateOneIteration(ug, theta, rng_est);
            cout << "  One DecreaseESComputation iteration: " << oneIter << "s" << endl;

            for (int k : k_values) {
                // AG estimate: k iterations
                double ag_est = oneIter * k;
                // GR estimate: ~2k iterations
                double gr_est = oneIter * k * 2;

                cout << "\n  k=" << k << " | AG est: " << ag_est << "s, GR est: " << gr_est << "s" << endl;

                // AdvancedGreedy
                if (ag_est > day_limit) {
                    cout << "  [AG] SKIP: estimated " << ag_est << "s > 1 day (" << day_limit << "s)" << endl;
                    logOut << "AG\t" << dsName << "\t" << k << "\t" << default_s
                           << "\t-\t-\t-\t" << ag_est << "\tSKIP_ESTIMATED_>1DAY" << endl;
                } else {
                    mt19937 rng_ag(123);
                    AlgoResult res = AdvancedGreedy(ug, seeds, k, theta, mc_rounds, rng_ag, time_limit);
                    cout << "  [AG] saved=" << res.saved_nodes << " time=" << res.time_seconds << "s" << endl;
                    logOut << "AG\t" << dsName << "\t" << k << "\t" << default_s
                           << "\t" << res.spread_before << "\t" << res.spread_after
                           << "\t" << res.saved_nodes << "\t" << res.time_seconds << "\tOK" << endl;
                    logOut.flush();
                }

                // GreedyReplace
                if (gr_est > day_limit) {
                    cout << "  [GR] SKIP: estimated " << gr_est << "s > 1 day (" << day_limit << "s)" << endl;
                    logOut << "GR\t" << dsName << "\t" << k << "\t" << default_s
                           << "\t-\t-\t-\t" << gr_est << "\tSKIP_ESTIMATED_>1DAY" << endl;
                } else {
                    mt19937 rng_gr(456);
                    AlgoResult res = GreedyReplace(ug, seeds, k, theta, mc_rounds, rng_gr, time_limit);
                    cout << "  [GR] saved=" << res.saved_nodes << " time=" << res.time_seconds << "s" << endl;
                    logOut << "GR\t" << dsName << "\t" << k << "\t" << default_s
                           << "\t" << res.spread_before << "\t" << res.spread_after
                           << "\t" << res.saved_nodes << "\t" << res.time_seconds << "\tOK" << endl;
                    logOut.flush();
                }
            }
        }

        // Experiment 2: Vary |S| with fixed k=100
        cout << "\n--- Experiment 2: Vary |S|, k=" << default_k << " ---" << endl;
        for (int s : s_values) {
            if (s == default_s) {
                cout << "  |S|=" << s << " already tested above, skipping duplicate." << endl;
                continue;
            }

            mt19937 rng_seed(42 + s);
            string seedFile = datadir + "/" + baseName + "_seed_" + to_string(s) + ".txt";
            vector<int> seeds = loadOrGenerateSeeds(seedFile, g, s, rng_seed);

            UnifiedGraph ug;
            ug.build(g, seeds);

            mt19937 rng_est(789 + s);
            double oneIter = estimateOneIteration(ug, theta, rng_est);
            double ag_est = oneIter * default_k;
            double gr_est = oneIter * default_k * 2;

            cout << "\n  |S|=" << s << " | AG est: " << ag_est << "s, GR est: " << gr_est << "s" << endl;

            // AdvancedGreedy
            if (ag_est > day_limit) {
                cout << "  [AG] SKIP: estimated > 1 day" << endl;
                logOut << "AG\t" << dsName << "\t" << default_k << "\t" << s
                       << "\t-\t-\t-\t" << ag_est << "\tSKIP_ESTIMATED_>1DAY" << endl;
            } else {
                mt19937 rng_ag(123 + s);
                AlgoResult res = AdvancedGreedy(ug, seeds, default_k, theta, mc_rounds, rng_ag, time_limit);
                cout << "  [AG] saved=" << res.saved_nodes << " time=" << res.time_seconds << "s" << endl;
                logOut << "AG\t" << dsName << "\t" << default_k << "\t" << s
                       << "\t" << res.spread_before << "\t" << res.spread_after
                       << "\t" << res.saved_nodes << "\t" << res.time_seconds << "\tOK" << endl;
                logOut.flush();
            }

            // GreedyReplace
            if (gr_est > day_limit) {
                cout << "  [GR] SKIP: estimated > 1 day" << endl;
                logOut << "GR\t" << dsName << "\t" << default_k << "\t" << s
                       << "\t-\t-\t-\t" << gr_est << "\tSKIP_ESTIMATED_>1DAY" << endl;
            } else {
                mt19937 rng_gr(456 + s);
                AlgoResult res = GreedyReplace(ug, seeds, default_k, theta, mc_rounds, rng_gr, time_limit);
                cout << "  [GR] saved=" << res.saved_nodes << " time=" << res.time_seconds << "s" << endl;
                logOut << "GR\t" << dsName << "\t" << default_k << "\t" << s
                       << "\t" << res.spread_before << "\t" << res.spread_after
                       << "\t" << res.saved_nodes << "\t" << res.time_seconds << "\tOK" << endl;
                logOut.flush();
            }
        }
    }

    logOut << "=== Experiment finished at " << time(nullptr) << " ===" << endl;
    logOut.close();

    cout << "\n========================================" << endl;
    cout << "All experiments completed. Results saved to: " << logFile << endl;
    cout << "========================================" << endl;

    return 0;
}
