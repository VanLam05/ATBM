/**
 * Convert dataset from the common format (n m / directed|undirected / edges)
 * to the IMin (SandIMIN) format:
 *   - graph.txt: u v p (edge with WC probability)
 *   - attribute.txt: n=X \n m=X
 *   - rumorSet_X.txt: seed nodes (one per line)
 *
 * After running this, use el2bin to convert graph.txt to graph_ic.inf:
 *   cd <output_dir> && ../../SandIMIN_code/el2bin graph.txt graph_ic
 *
 * Compile:
 *   g++ -O3 -std=c++17 -o prepare_dataset prepare_dataset.cpp
 *
 * Usage:
 *   ./prepare_dataset -input <dataset.txt> -output <output_dir> -seedNum <|S|>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    string inputFile = "";
    string outputDir = "";
    int seedNum = 10;

    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "-input") inputFile = argv[++i];
        else if (string(argv[i]) == "-output") outputDir = argv[++i];
        else if (string(argv[i]) == "-seedNum") seedNum = atoi(argv[++i]);
    }

    if (inputFile.empty() || outputDir.empty()) {
        cerr << "Usage: ./prepare_dataset -input <dataset.txt> -output <output_dir> [-seedNum |S|]" << endl;
        return 1;
    }

    // Create output directory
    fs::create_directories(outputDir);

    // Read input
    ifstream fin(inputFile);
    if (!fin.is_open()) {
        cerr << "Cannot open: " << inputFile << endl;
        return 1;
    }

    int n, m;
    fin >> n >> m;
    string type;
    fin >> type;
    bool directed = (type == "directed");

    vector<pair<int,int>> raw_edges;
    for (int i = 0; i < m; i++) {
        int u, v;
        fin >> u >> v;
        if (u < 0 || u >= n || v < 0 || v >= n) continue;
        if (u == v) continue;
        raw_edges.push_back({u, v});
    }
    fin.close();

    // If undirected, add both directions
    vector<pair<int,int>> edges;
    if (!directed) {
        for (auto& [u, v] : raw_edges) {
            edges.push_back({u, v});
            edges.push_back({v, u});
        }
    } else {
        edges = raw_edges;
    }

    // Compute in-degrees for WC model
    vector<int> in_degree(n, 0);
    vector<int> out_degree(n, 0);
    for (auto& [u, v] : edges) {
        in_degree[v]++;
        out_degree[u]++;
    }

    int total_edges = (int)edges.size();

    // Write attribute.txt
    {
        ofstream fout(outputDir + "/attribute.txt");
        fout << "n=" << n << "\n";
        fout << "m=" << total_edges << "\n";
        fout.close();
        cout << "Written attribute.txt: n=" << n << " m=" << total_edges << endl;
    }

    // Write graph.txt with WC probabilities
    {
        ofstream fout(outputDir + "/graph.txt");
        for (auto& [u, v] : edges) {
            double p = 1.0 / in_degree[v];
            fout << u << " " << v << " " << p << "\n";
        }
        fout.close();
        cout << "Written graph.txt with " << total_edges << " edges" << endl;
    }

    // Generate seed set: top-200 by out-degree, randomly pick seedNum
    {
        vector<int> indices(n);
        iota(indices.begin(), indices.end(), 0);
        partial_sort(indices.begin(),
                     indices.begin() + min(200, n),
                     indices.end(),
                     [&](int a, int b) { return out_degree[a] > out_degree[b]; });

        int topK = min(200, n);
        vector<int> top200(indices.begin(), indices.begin() + topK);

        mt19937 rng(42);
        shuffle(top200.begin(), top200.end(), rng);

        int actualSeedNum = min(seedNum, topK);
        vector<int> seeds(top200.begin(), top200.begin() + actualSeedNum);

        // Write for multiple seed sizes
        vector<int> seed_sizes = {10, 20, 30, 40, 50};
        for (int s : seed_sizes) {
            if (s > topK) s = topK;
            ofstream fout(outputDir + "/rumorSet_" + to_string(s) + ".txt");
            for (int i = 0; i < s && i < (int)top200.size(); i++) {
                fout << top200[i] << "\n";
            }
            fout.close();
            cout << "Written rumorSet_" << s << ".txt" << endl;
        }

        // Also save to seed_node.txt for reference
        ofstream fout(outputDir + "/seed_node.txt");
        for (int i = 0; i < (int)top200.size(); i++) {
            fout << top200[i] << "\n";
        }
        fout.close();
    }

    // Write graph_ic.inf binary file directly
    // Format: for each edge, write (int u, int v, double p) = 16 bytes
    {
        ofstream fout(outputDir + "/graph_ic.inf", ios::binary);
        for (auto& [u, v] : edges) {
            double p = 1.0 / in_degree[v];
            int a = u, b = v;
            fout.write(reinterpret_cast<char*>(&a), sizeof(int));
            fout.write(reinterpret_cast<char*>(&b), sizeof(int));
            fout.write(reinterpret_cast<char*>(&p), sizeof(double));
        }
        fout.close();
        cout << "Written graph_ic.inf (binary) with " << total_edges << " edges ("
             << total_edges * 16 << " bytes)" << endl;
    }

    cout << "\nDataset prepared in: " << outputDir << endl;
    cout << "Ready to run IMin with: ./IMIN -dataset " << outputDir << " ..." << endl;

    return 0;
}
