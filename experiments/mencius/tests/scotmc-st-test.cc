#include <unistd.h>
#include <iostream>

#include <thread>
#include <random>
#include <utility>
#include <vector>

#include <functional>

#include "../../../src/includes/scot-core.hh"
#include "../includes/menc-core.hh"

#ifdef __DEBUG__
#define REQ_NUM     3000
#else
#define REQ_NUM     1000000
#endif

void generate_random_str(std::mt19937& generator, char* buffer, int len) {

    // Printable ASCII range.
    const char* character_set = 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    std::uniform_int_distribution<int> distributor(0, 61);
    for (int i = 0; i < len; i++)
        buffer[i] = character_set[static_cast<char>(distributor(generator))];
}


void worker(int nid, int tid, int gen_sz, int key_sz, scot_menc::ScotMenciusCore& core_instance) {

    std::string logger_name("wrkr-");
    logger_name += std::to_string(tid);
    
    scot::MessageOut m_out(logger_name.c_str());

    char static_buffer[4096] = { 0, };

    m_out.get_logger()->info("{} spawned.", logger_name);

    std::mt19937 generator((unsigned int)time(NULL));
    std::hash<std::string> hm;

    scot::ScotTimestamp ts(logger_name + ".csv");

    uint32_t hashv;
    sleep(3); // Wait for a bit.

    for (int i = 0; i < REQ_NUM; i++) {

        // Propose
        generate_random_str(generator, static_buffer, gen_sz);
        hashv = hm(std::string(static_buffer));

        uint64_t index = ts.record_start();
        core_instance.propose(
            (uint8_t*)static_buffer, gen_sz, 
            (uint8_t*)static_buffer, key_sz, 
            hashv
        );
        ts.record_end(index);

        std::memset(static_buffer, 0, 4096);
#ifdef __DEBUG__
        if (i % 500 == 0) {
#else
        if (i % 50000 == 0) {
#endif
            float perc = ((i * 100) / REQ_NUM);
            m_out.get_logger()->info("PROPOSE: {:03.2f}%", perc);
        }
    }

    sleep(1);

    return;
}


int main(int argc, char* argv[]) {

    const int pld_sz    = atoi(argv[1]);
    const int key_sz    = atoi(argv[2]);

    scot_menc::ScotMenciusCore scot_menc_instance;
    scot_menc_instance.initialize();

    int nid = scot::ScotConnection::get_instance().get_my_nid();

    std::thread standalone_test_wrkr(
        worker, nid, 0, (pld_sz + key_sz), key_sz, std::ref(scot_menc_instance));

    standalone_test_wrkr.join();

    scot_menc_instance.finish();
    
    std::cout << "Test end. \n";

    return 0;
}