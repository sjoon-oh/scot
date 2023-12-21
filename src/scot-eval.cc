

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


uint64_t scot::ScotTimestamp::record_start() {

    uint64_t index = next.fetch_add(1);

    clock_gettime(CLOCK_MONOTONIC, &start[index]);
    return index;
}


void scot::ScotTimestamp::record_end(uint64_t index) {

    if (index > (SCOT_TIMESTAMP_RECORDS - 1)) 
        return;

    clock_gettime(CLOCK_MONOTONIC, &end[index]);
}


void scot::ScotTimestamp::dump_to_fs() {
    
    uint64_t records = next.load();

    struct timespec* elapsed = new struct timespec[records];
    FILE* fp = fopen(fname.c_str(), "w");

    long long int time;
    for (int i = 0; i < records; i++) {

        time = __ELAPSED_NSEC__(start[i], end[i]);
        fprintf(fp, "%lld\n", time);
    }

    delete[] elapsed;
}
