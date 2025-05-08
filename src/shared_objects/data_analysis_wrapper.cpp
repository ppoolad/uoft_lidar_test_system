#include "../data_analysis/data_analysis.h"

using namespace std;

extern "C" int extern_main(const char* file, int packet_number, const char* output_name, int adjust_start, int nbins) {
    vector<int> memory_words = extract_memory_words(file, output_name, packet_number, adjust_start, nbins);
    
    if (memory_words.empty()) {
        return 1;
    }

    run_histogram_script(memory_words, output_name, nbins);
    return 0;
}
