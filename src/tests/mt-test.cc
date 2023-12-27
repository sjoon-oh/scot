#include <unistd.h>
#include <iostream>

#include <thread>
#include <random>
#include <utility>
#include <vector>

#include <functional>


#include "../includes/scot-core.hh"

#define REQ_NUM     1000000

void generate_random_str(std::mt19937& generator, char* buffer, int len) {

    // Printable ASCII range.
    const char* character_set = 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    std::uniform_int_distribution<int> distributor(0, 61);
    for (int i = 0; i < len; i++)
        buffer[i] = character_set[static_cast<char>(distributor(generator))];
}

void worker(int nid, int tid, int gen_sz, int key_sz, scot::ScotCore& core_instance) {

    std::string logger_name("wrkr-");
    logger_name += std::to_string(tid);
    
    scot::MessageOut m_out(logger_name.c_str());

    char static_buffer[4096] = { 0, };

    m_out.get_logger()->info("{} spawned.", logger_name);

    std::mt19937 generator;
    std::hash<std::string> hm;

    scot::ScotTimestamp ts(logger_name + ".csv");


    sleep(3); // Wait for a bit.

    if (nid == 0) {
        for (int i = 0; i < REQ_NUM; i++) {

            // Propose
            generate_random_str(generator, static_buffer, gen_sz);

            uint64_t index = ts.record_start();
            core_instance.propose(
                (uint8_t*)static_buffer, gen_sz, 
                (uint8_t*)static_buffer, key_sz, 
                hm(std::string(static_buffer)), 
                SCOT_MSGTYPE_PURE
            );
            ts.record_end(index);

            std::memset(static_buffer, 0, 4096);

            if (i % 50000 == 0) {
                float perc = (i / REQ_NUM) * 100;
                m_out.get_logger()->info("PROPOSE: {:03.2f}%", perc);
            }
        }
    }
    else
        sleep(10);

    return;
}

int main(int argc, char* argv[]) {

    const int pld_sz    = atoi(argv[1]);
    const int key_sz    = atoi(argv[2]);
    const int thr_num   = atoi(argv[3]);

    std::vector<std::thread> thr_vec;

    //
    // Start core instance
    scot::ScotCore scot_instance;
    scot_instance.initialize();

    int nid = scot::ScotConnection::get_instance().get_my_nid();
    // int qsz = scot::ScotConnection::get_instance().get_quorum_sz();

    // Logger
    for (int i = 0; i < thr_num; i++) {
        thr_vec.push_back(std::move(
            std::thread(worker, nid, i, (pld_sz + key_sz), key_sz, std::ref(scot_instance))));
    }

    for (auto& thr: thr_vec) thr.join();

    scot_instance.finish();
    
    std::cout << "Test end. \n";

    return 0;
}