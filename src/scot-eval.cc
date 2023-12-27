#include <algorithm>

#include "./includes/scot-eval.hh"


scot::ScotTimestamp::ScotTimestamp(std::string name, uint64_t arr_sz) 
    : fname(name), rec_sz(arr_sz), start(nullptr), end(nullptr) {
    next.store(0);

    if (arr_sz != 0) {
        start = new struct timespec[rec_sz];
        end = new struct timespec[rec_sz];
    }
}


scot::ScotTimestamp::~ScotTimestamp() {

    dump_to_fs();

    if (start != nullptr) delete[] start;
    if (end != nullptr) delete[] end;
}


void scot::ScotTimestamp::dump_to_fs() {
    
    uint64_t records = next.load();

    long long int* elapsed = new long long int[records];
    FILE* fp = fopen(fname.c_str(), "w");

    // Form first.
    for (int i = 0; i < records; i++)
        elapsed[i] = __ELAPSED_NSEC__(start[i], end[i]);
    
    __dump_statistics(fname, elapsed, records);

    // Dump.
    for (int i = 0; i < records; i++)
        fprintf(fp, "%lf\n", (elapsed[i] * 0.001));

    fclose(fp);

    

    delete[] elapsed;
}


void scot::ScotTimestamp::__dump_statistics(std::string& fname, long long int* elapsed, uint64_t records) {

    std::string stat_fname = fname;
    stat_fname.insert(0, "stat-");

    FILE* fp = fopen(stat_fname.c_str(), "w");

    std::sort(elapsed, elapsed + records);

    uint64_t median = records / 2;
    uint64_t front_1 = records * 0.01;
    uint64_t tail_99 = records * 0.99;
    
    fprintf(fp, "total\t1\%ile\t50\%ile\t99\%ile\n");
    fprintf(fp, "%lld\t%lf\t%lf\t%lf\n", 
        records,
        (elapsed[front_1] * 0.001),
        (elapsed[median] * 0.001),
        (elapsed[tail_99] * 0.001)
    );

    fclose(fp);
}