
#include "sfmt/SFMT.h"
#include "head.h"
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

class Argument
{
public:
    unsigned int k;
    string dataset;
    string dataset_dir;
    string dataset_name;
    string seedFile;
    string outputDir;
    string outputFile;
    string res;
    string algo;
    int Rumor_num;
    double delta;
    double gamma;
    double beta;
    double epsilon;
};

#include "graph.h"
#include "infgraph.h"
#include "Sandwich.h"

void OutputSeedSetToFile(vector<int> seed_set, const Argument &arg)
{
    string seedfile = "results/res_" + arg.dataset;
    ofstream of(seedfile, ios::app);
    // of.open(seedfile);
    for (int seed : seed_set)
    {
        of << seed << endl;
    }
    of << endl;
    of.close();
}

void run_with_parameter(InfGraph &g, Argument &arg)
{
    CP::CP_based(g, arg);

    // OutputSeedSetToFile(g.seedSet, arg);
}
void Run(int argn, char **argv)
{
    Argument arg;

    arg.k = 0;

    for (int i = 0; i < argn; i++)
    {
        if (argv[i] == string("-k"))
            arg.k = atoi(argv[i + 1]);
        if (argv[i] == string("-dataset"))
            arg.dataset = argv[i + 1];
        if (argv[i] == string("-seedFile"))
            arg.seedFile = argv[i + 1];
        if (argv[i] == string("-outputDir"))
            arg.outputDir = argv[i + 1];
        if (argv[i] == string("-output"))
            arg.outputFile = argv[i + 1];
        if (argv[i] == string("-algo"))
            arg.algo = argv[i + 1];
        if (argv[i] == string("-rumorNum"))
            arg.Rumor_num = atoi(argv[i + 1]);
        if (argv[i] == string("-delta"))
            arg.delta = stod(argv[i + 1]);
        if (argv[i] == string("-gamma"))
            arg.gamma = stod(argv[i + 1]);
        if (argv[i] == string("-beta"))
            arg.beta = stod(argv[i + 1]);
        if (argv[i] == string("-epsilon"))
            arg.epsilon = stod(argv[i + 1]);
    }
    ASSERT(arg.dataset != "");

    string dataset_path = arg.dataset;
    string normalized_path = dataset_path;
    for (char &ch : normalized_path)
        if (ch == '\\')
            ch = '/';

    size_t slash_pos = normalized_path.find_last_of("/");
    string temp_name = (slash_pos == string::npos) ? normalized_path : normalized_path.substr(slash_pos + 1);
    bool common_dataset = false;
    string lower_name = temp_name;
    transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
    if (lower_name.size() >= 4 && lower_name.substr(lower_name.size() - 4) == ".txt")
        common_dataset = true;

    if (common_dataset)
    {
        size_t dot_pos = temp_name.find_last_of(".");
        arg.dataset_name = (dot_pos == string::npos) ? temp_name : temp_name.substr(0, dot_pos);
        arg.dataset_dir = "";
        if (arg.outputDir.empty())
        {
            fs::path dataset_fs_path(normalized_path);
            fs::path dataset_dir = dataset_fs_path.parent_path();
            fs::path project_root = dataset_dir.filename() == "datasets" ? dataset_dir.parent_path() : dataset_dir;
            arg.outputDir = (project_root / "results").generic_string();
        }
    }
    else
    {
        arg.dataset_name = temp_name;
        arg.dataset_dir = dataset_path;
        if (!arg.dataset_dir.empty() && arg.dataset_dir.back() != '/' && arg.dataset_dir.back() != '\\')
            arg.dataset_dir += "/";
        if (arg.outputDir.empty())
            arg.outputDir = "results";
    }

    fs::create_directories(arg.outputDir);
    if (!arg.outputFile.empty())
    {
        arg.res = arg.outputFile;
        fs::path output_parent = fs::path(arg.res).parent_path();
        if (!output_parent.empty())
            fs::create_directories(output_parent);
    }
    else
    {
        fs::path output_path = fs::path(arg.outputDir) / ("res_" + arg.dataset_name + "_|S|=" + to_string(arg.Rumor_num) + "_K=" + to_string(arg.k) + "_epsilon=" + to_string(arg.epsilon) + "_gamma=" + to_string(arg.gamma) + "_beta=" + to_string(arg.beta) + "_algo=" + arg.algo);
        arg.res = output_path.generic_string();
    }
    cout << "Output file: " << arg.res << endl;

    string graph_file = common_dataset ? dataset_path : arg.dataset_dir + "graph_ic.inf";
    InfGraph g(arg.dataset_dir, graph_file, common_dataset);
    run_with_parameter(g, arg);
}

int main(int argn, char **argv)
{
    __head_version = "v1";
    OutputInfo info(argn, argv);

    Run(argn, argv);
}
