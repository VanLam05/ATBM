#include <string>
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using namespace std;

//this one gives the high water mark memory

unsigned int getProcMemory(){
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS info;
	if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)))
		return static_cast<unsigned int>(info.PeakWorkingSetSize / 1024.0 / 1024.0);
	return 0;
#else
	struct rusage r_usage;
	getrusage(RUSAGE_SELF, &r_usage);
	//string strMemUsage = to_string(r_usage.ru_maxrss);
	return r_usage.ru_maxrss / 1024.0 ;
	//return strMemUsage;
#endif
}
