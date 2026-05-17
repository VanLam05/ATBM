#pragma once
#pragma once
#include <chrono>
#include <ctime>
#include <ratio>
#include <queue>
#include <ctime>
#include <random>
#include <numeric>
//#include "infgraph.h"
#define e exp(1)
#define c 2*(exp(1)-2)

using namespace std::chrono;

class Math {
public:
	static double log2(int n) {
		return log(n) / log(2);
	}
	static double logcnk(int n, int k) {
		double ans = 0;
		for (int i = n - k + 1; i <= n; i++)
		{
			ans += log(i);
		}
		for (int i = 1; i <= k; i++)
		{
			ans -= log(i);
		}
		return ans;
	}
};

class CP
{
private:


public:
	
	static bool load_seed_file(const string& seed_file, InfGraph& g)
	{
		if (seed_file.empty())
			return false;
		ifstream inFile(seed_file);
		if (!inFile.is_open())
			return false;

		int number;
		while (inFile >> number)
		{
			if (number >= 0 && number < (int)g.n)
			{
				g.rumorSet.push_back(number);
				g.isRumor[number] = true;
			}
		}
		inFile.close();
		return !g.rumorSet.empty();
	}

	static void generate_seed_file(const string& seed_file, InfGraph& g, int seed_num)
	{
		vector<int> indices(g.n);
		iota(indices.begin(), indices.end(), 0);
		partial_sort(indices.begin(),
			indices.begin() + min(200, (int)g.n),
			indices.end(),
			[&](int a, int b) { return g.deg[a] > g.deg[b]; });

		int topK = min(200, (int)g.n);
		vector<int> top200(indices.begin(), indices.begin() + topK);
		mt19937 rng(42);
		shuffle(top200.begin(), top200.end(), rng);

		int actualSeedNum = min(seed_num, topK);
		ofstream fout(seed_file);
		for (int i = 0; i < actualSeedNum; i++)
			fout << top200[i] << "\n";
		fout.close();
	}

	static void load_or_generate_rumor_set(InfGraph& g, Argument& arg)
	{
		g.rumorSet.clear();
		fill(g.isRumor.begin(), g.isRumor.end(), false);

		if (!arg.seedFile.empty() && load_seed_file(arg.seedFile, g))
		{
			cout << "Loaded " << g.rumorSet.size() << " seeds from " << arg.seedFile << endl;
			g.isRumor[g.n] = true;
			return;
		}

		string rumor_file = "";
		if (!arg.dataset_dir.empty())
			rumor_file = arg.dataset_dir + "rumorSet_" + to_string(arg.Rumor_num) + ".txt";

		if (!rumor_file.empty() && load_seed_file(rumor_file, g))
		{
			cout << "Loaded " << g.rumorSet.size() << " seeds from " << rumor_file << endl;
			g.isRumor[g.n] = true;
			return;
		}

		if (arg.seedFile.empty())
			arg.seedFile = arg.outputDir + "/" + arg.dataset_name + "_seed_node_" + to_string(arg.Rumor_num) + ".txt";

		generate_seed_file(arg.seedFile, g, arg.Rumor_num);
		if (!load_seed_file(arg.seedFile, g))
		{
			cerr << "Unable to load or generate seed file: " << arg.seedFile << endl;
			exit(1);
		}
		cout << "Generated " << g.rumorSet.size() << " seeds, saved to " << arg.seedFile << endl;
		g.isRumor[g.n] = true;
	}


	static void CP_based(InfGraph& g, Argument& arg)
	{

		sfmt_t sfmtSeed;
		sfmt_init_gen_rand(&sfmtSeed, rand());

		double total_spread = 0;
		double total_time = 0;
		load_or_generate_rumor_set(g, arg);
		vector<int>CB;//out_neighbor set
		vector<int>is_ON(g.n, 0);
		for (int node : g.rumorSet)
		{
			for (int i = 0; i < g.gT[node].size(); i++)
			{
				int v = g.gT[node][i];
				if (!is_ON[v])
				{
					CB.push_back(v);
					is_ON[v] = 1;
				}
			}
		}
		ofstream of(arg.res, ios::app);
		g.init_hyper_graph();
		double inf1 = g.MC_based_estimate(g.rumorSet, 10000);
		cout << "estimate influence by Mente-Carlo is: " << inf1 << endl;
		g.get_reachable_node(g.Rnode);
		cout << "reachable node size: " << g.Rnode.size() << endl;
		high_resolution_clock::time_point startTime = high_resolution_clock::now();
		int CPnum = 10000;
		if (arg.algo == "SandIMIN")
		{
			if (CB.size() <= arg.k)
			{
				g.seedSet = CB;
			}
			else
			{
				g.seedSet.clear();
				high_resolution_clock::time_point startTime1 = high_resolution_clock::now();
				double inf = g.estimate_inf_byStop(g.rumorSet, g.seedSet, arg.beta, 1.0 / g.n / 6);
				high_resolution_clock::time_point endTime1 = high_resolution_clock::now();
				duration<double> interval = duration_cast<duration<double>>(endTime1 - startTime1);
				cout << "estimate influence time:" << interval.count() << endl;

				cout << "estimate influence by StopAlgorithm is: " << inf << endl;
				double OPT_LB = g.calculate_OPT_lower(arg.k, CB);

				g.opimc_sandwich(arg.k, arg.epsilon, 1.0 / g.n, arg, inf, OPT_LB, 0, 0);
				g.deg_based_heuristic(arg.k, CB);
				g.reset_pro();

				double inf_upper = g.estimate_inf_byStop(g.rumorSet, g.UB_seedSet, arg.gamma, 1.0 / g.n);
				cout << "upper's influence:" << inf_upper << endl;

				double inf_lower = g.estimate_inf_byStop(g.rumorSet, g.LB_seedSet, arg.gamma, 1.0 / g.n);
				cout << "lower's influence:" << inf_lower << endl;

				double inf_or = g.estimate_inf_byStop(g.rumorSet, g.Or_seedSet, arg.gamma, 1.0 / g.n);
				cout << "orginal's influence:" << inf_or << endl;
				if (inf_upper <= inf_lower && inf_upper <= inf_or)
					g.seedSet = g.UB_seedSet;
				else if (inf_lower <= inf_upper && inf_lower <= inf_or)
					g.seedSet = g.LB_seedSet;
				else if (inf_or <= inf_lower && inf_or <= inf_upper)
					g.seedSet = g.Or_seedSet;
			}
		}
		else if (arg.algo == "SandIMIN-")
		{
			if (CB.size() <= arg.k)
			{
				g.seedSet = CB;
			}
			else
			{
				g.seedSet.clear();
				high_resolution_clock::time_point startTime1 = high_resolution_clock::now();
				double inf = g.estimate_inf_byStop(g.rumorSet, g.seedSet, arg.gamma, 1.0 / g.n / 6);
				high_resolution_clock::time_point endTime1 = high_resolution_clock::now();
				duration<double> interval = duration_cast<duration<double>>(endTime1 - startTime1);
				cout << "estimate influence time:" << interval.count() << endl;
				cout << "estimate influence by StopAlgorithm is: " << inf << endl;
				double OPT_LB = g.calculate_OPT_lower(arg.k, CB);

				g.opimc_sandwich(arg.k, arg.epsilon, 1.0 / g.n, arg, inf, OPT_LB, 1, 0);
				
				g.deg_based_heuristic(arg.k, CB);
				g.reset_pro();

				double inf_lower = g.estimate_inf_byStop(g.rumorSet, g.LB_seedSet, arg.beta, 1.0 / g.n);
				cout << "lower's influence:" << inf_lower << endl;

				double inf_or = g.estimate_inf_byStop(g.rumorSet, g.Or_seedSet, arg.beta, 1.0 / g.n);
				cout << "orginal's influence:" << inf_or << endl;
				if (inf_lower <= inf_or)
					g.seedSet = g.LB_seedSet;
				else
					g.seedSet = g.Or_seedSet;
			}
		}
		high_resolution_clock::time_point endTime = high_resolution_clock::now();
		duration<double> interval = duration_cast<duration<double>>(endTime - startTime);
		total_time += (double)interval.count();
		cout << "time:" << interval.count() << endl;

		// Output the best result (original behavior)
		g.reset_pro();
		g.Delete_Node(g.seedSet);
		double inf2 = g.MC_based_estimate(g.rumorSet, 100000);
		double inf3 = inf1 - inf2;
		of << "BEST\t" << inf1 << "\t" <<  inf2 << "\t" << inf3 << "\t" << (double)interval.count() <<  endl;
		cout << "Best result: spread_before=" << inf1 << " spread_after=" << inf2 << " saved=" << inf3 << endl;

		// Output separate lower bound result
		if (arg.algo == "SandIMIN" && g.LB_seedSet.size() > 0) {
			g.reset_pro();
			g.Delete_Node(g.LB_seedSet);
			double inf2_lb = g.MC_based_estimate(g.rumorSet, 100000);
			double inf3_lb = inf1 - inf2_lb;
			of << "LB\t" << inf1 << "\t" << inf2_lb << "\t" << inf3_lb << "\t" << (double)interval.count() << endl;
			cout << "Lower bound: spread_after=" << inf2_lb << " saved=" << inf3_lb << endl;
		}

		// Output separate upper bound result
		if (arg.algo == "SandIMIN" && g.UB_seedSet.size() > 0) {
			g.reset_pro();
			g.Delete_Node(g.UB_seedSet);
			double inf2_ub = g.MC_based_estimate(g.rumorSet, 100000);
			double inf3_ub = inf1 - inf2_ub;
			of << "UB\t" << inf1 << "\t" << inf2_ub << "\t" << inf3_ub << "\t" << (double)interval.count() << endl;
			cout << "Upper bound: spread_after=" << inf2_ub << " saved=" << inf3_ub << endl;
		}

		// Output heuristic (original) result
		if (arg.algo == "SandIMIN" && g.Or_seedSet.size() > 0) {
			g.reset_pro();
			g.Delete_Node(g.Or_seedSet);
			double inf2_or = g.MC_based_estimate(g.rumorSet, 100000);
			double inf3_or = inf1 - inf2_or;
			of << "OR\t" << inf1 << "\t" << inf2_or << "\t" << inf3_or << "\t" << (double)interval.count() << endl;
			cout << "Heuristic: spread_after=" << inf2_or << " saved=" << inf3_or << endl;
		}

		of.close();
	}
};
