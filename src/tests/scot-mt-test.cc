#include <unistd.h>
#include <iostream>

#include <thread>
#include <random>
#include <utility>
#include <vector>

#include <functional>
#include <cstdlib>

#include "../sample/scot-balance-rule.hh"
#include "../includes/scot-core.hh"

#ifdef __DEBUG__
#define REQ_NUM     10000
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

void worker(int nid, int tid, int gen_sz, int key_sz, int req_num, scot::ScotCore& core_instance) {

    std::string logger_name("wrkr-");
    logger_name += std::to_string(tid);
    
    scot::MessageOut m_out(logger_name.c_str());

    char** static_buffer = (char**)std::malloc(sizeof(char*) * req_num);
    uint32_t* hashv = (uint32_t*)std::malloc(sizeof(uint32_t) * req_num);

    m_out.get_logger()->info("{} spawned.", logger_name);

    std::mt19937 generator((unsigned int)time(NULL) + tid);
    std::hash<std::string> hm;

    scot::ScotTimestamp ts(logger_name + ".csv");
    scot::ScotTimestamp ts_runtime(logger_name + "-runtime.csv");

    for (int i = 0; i < req_num; i++) {
        char* buffer = (char*)std::malloc(sizeof(char) * 4096);
        std::memset(buffer, 0, 4096);

        generate_random_str(generator, buffer, gen_sz);
        hashv[i] = hm(std::string(buffer));

        static_buffer[i] = buffer;
    }

    m_out.get_logger()->info("Payload generated.");

    sleep(3); // Wait for a bit.

    uint64_t rt_index = ts_runtime.record_start();
    for (int i = 0; i < req_num; i++) {

        uint64_t index = ts.record_start();
        core_instance.propose(
            (uint8_t*)(static_buffer[i]), gen_sz, 
            (uint8_t*)(static_buffer[i]), key_sz, 
            hashv[i]
        );
        ts.record_end(index);

        // if (i % 50000 == 0) {
        //     float perc = ((i * 100) / REQ_NUM);
        //     m_out.get_logger()->info("PROPOSE: {:03.2f}%", perc);
        // }
    }
    ts_runtime.record_end(rt_index);

    sleep(20);

    for (int i = 0; i < req_num; i++) {
        std::free(static_buffer[i]);
    }

    std::free(static_buffer);
    std::free(hashv);

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

    scot::rule_balance_init(scot_instance.get_qsize());

    scot_instance.add_rule(2, scot::rule_balance_2);
    scot_instance.add_rule(3, scot::rule_balance_3);

    scot_instance.update_active(3);

    std::cout << "Rule added.\n";

    scot_instance.initialize();

    int req_num = REQ_NUM / thr_num;
    int nid = scot::ScotConnection::get_instance().get_my_nid();
    // int qsz = scot::ScotConnection::get_instance().get_quorum_sz();

    // Logger
    for (int i = 0; i < thr_num; i++) {
        thr_vec.push_back(std::move(
            std::thread(worker, nid, i, (pld_sz + key_sz), key_sz, req_num, std::ref(scot_instance))));
    }

    std::cout << "Waiting for join.\n";

    for (auto& thr: thr_vec) thr.join();

    scot_instance.finish();
    
    std::cout << "Test end. \n";

    return 0;
}