#define HEAD_INFO
#include "sfmt/SFMT.h"
#include "head.h"

#include "MeasureM.h"
#include "memoryusage.h"

using namespace std;
typedef double (*pf)(int, int);
void handle_error(const char* msg);

class Graph
{
public:    	
	unsigned int n, m;
    vector<int>deg;
    vector<int>inverse_deg;
    vector<vector<int>> gT;
    vector<vector<int>> gT_reverse;
    vector<vector<double>> probT_reverse;
	vector<vector<double>> probT;
    vector<vector<double>> probT2;
    string folder;
    string graph_file;
    bool common_format;
    void readNM()
    {
        if (common_format)
        {
            ifstream fin(graph_file.c_str());
            if (!fin.is_open())
                handle_error("open common dataset");
            fin >> n >> m;
            fin.close();
            return;
        }

        ifstream cin((folder + "attribute.txt").c_str());
        ASSERT(!cin == false);
        string s;
        while (cin >> s)
        {
            if (s.substr(0, 2) == "n=")
            {
                n = atoi(s.substr(2).c_str());
                continue;
            }
            if (s.substr(0, 2) == "m=")
            {
                m = atoi(s.substr(2).c_str());
                continue;
            }
            ASSERT(false);
        }
        TRACE(n, m );
        cin.close();
    }

    void readGraph()
    {
        ifstream fin(graph_file, ios::binary);
        if (!fin.is_open())
            handle_error("open graph_ic.inf");

        unsigned int readCnt = 0;
        for (unsigned int i = 0; i < m; i++)
        {
            readCnt ++;
            unsigned int a, b;
            double p;
            fin.read(reinterpret_cast<char*>(&a), sizeof(int));
            fin.read(reinterpret_cast<char*>(&b), sizeof(int));
            fin.read(reinterpret_cast<char*>(&p), sizeof(double));
            if (!fin)
                handle_error("read graph_ic.inf");

            ASSERT( a < n );
            ASSERT( b < n );
			
			
			probT[a].push_back(p);
            probT2[a].push_back(p);
			gT[a].push_back(b);
            deg[a]++;//out-degree
            inverse_deg[b]++;//inverse-degree
            gT_reverse[b].push_back(a);
            probT_reverse[b].push_back(p);
        }        

        ASSERT(readCnt == m);
        fin.close();
        
    }

    void readCommonGraph()
    {
        ifstream fin(graph_file.c_str());
        if (!fin.is_open())
            handle_error("open common dataset");

        unsigned int original_m;
        string type;
        fin >> n >> original_m;
        fin >> type;
        bool directed = (type == "directed");

        vector<pair<unsigned int, unsigned int>> raw_edges;
        raw_edges.reserve(original_m);
        for (unsigned int i = 0; i < original_m; i++)
        {
            unsigned int a, b;
            fin >> a >> b;
            if (!fin)
                break;
            if (a >= n || b >= n || a == b)
                continue;
            raw_edges.push_back(make_pair(a, b));
        }
        fin.close();

        vector<pair<unsigned int, unsigned int>> edges;
        if (directed)
        {
            edges = raw_edges;
        }
        else
        {
            edges.reserve(raw_edges.size() * 2);
            for (auto &edge : raw_edges)
            {
                edges.push_back(edge);
                edges.push_back(make_pair(edge.second, edge.first));
            }
        }

        m = static_cast<unsigned int>(edges.size());
        vector<unsigned int> in_degree(n, 0);
        for (auto &edge : edges)
            in_degree[edge.second]++;

        for (auto &edge : edges)
        {
            unsigned int a = edge.first;
            unsigned int b = edge.second;
            double p = 1.0 / in_degree[b];

            probT[a].push_back(p);
            probT2[a].push_back(p);
            gT[a].push_back(b);
            deg[a]++;
            inverse_deg[b]++;
            gT_reverse[b].push_back(a);
            probT_reverse[b].push_back(p);
        }
    }

    Graph(string folder, string graph_file, bool common_format = false): folder(folder), graph_file(graph_file), common_format(common_format)
    {		
		readNM();
        deg = vector<int>(n + 1);
        inverse_deg = vector<int>(n + 1);
		gT = vector<vector<int>>(n+2, vector<int>());
        gT_reverse = vector<vector<int>>(n+2, vector<int>());
        probT = vector<vector<double>>(n+2, vector<double>());
        probT2 = vector<vector<double>>(n+2, vector<double>());
        probT_reverse = vector<vector<double>>(n + 2, vector<double>());
        if (common_format)
            readCommonGraph();
        else
		    readGraph();
    }
};

double sqr(double t)
{
    return t * t;
}

void handle_error(const char* msg) 
{
	perror(msg);
	exit(255);
}

