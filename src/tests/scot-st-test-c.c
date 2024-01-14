/* Project SCOT
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 * https://github.com/sjoon-oh/
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>

#include "../includes/scot-core-c.h"
#include "../sample/scot-balance-rule-c.h"

#ifdef __DEBUG__
#define REQ_NUM     100
#else
#define REQ_NUM     1000000
#endif

int main(int argc, char* argv[]) {

    const int pld_sz    = atoi(argv[1]);
    const int key_sz    = atoi(argv[2]);

    void* scot_instance = 0;

    //
    // Start core instance
    scot_initialize();

    rule_balance_init_c(scot_get_qs());

    scot_add_rule(2, rule_balance_2_c);
    scot_add_rule(3, rule_balance_3_c);

    scot_update_active(3);
    printf("[C] Rule added.\n");

    srand(time(NULL));

    const char* character_set = 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    char buf[4096] = { 0, };
    uint32_t hashv = 0; // Represents a hash.

    int ts_handle = scot_timestamp_init("scot-st-testbin");

    for (int i = 0; i < REQ_NUM; i++) {

        for (int j = 0; j < (pld_sz + key_sz - 1); j++) {
            buf[j] = character_set[rand() % 63];
        }

        hashv = (uint32_t)rand();

        // printf("Buffer: %s, %ld\n", buf, hashv);

        uint64_t index = scot_timestamp_record_start(ts_handle);
        scot_propose(
            (uint8_t*)buf, (pld_sz + key_sz), (uint8_t*)buf, key_sz, hashv);
        scot_timestamp_record_end(ts_handle, index);

        memset(buf, 0, 4096);

        if (i % 50000 == 0) {
            float perc = ((i * 100) / REQ_NUM);
            printf("PROPOSE: %03.2f%%, HASH: %ld\n", perc, hashv);
        }
    }

    scot_finalize();
    printf("[C] Test end, waiting...");

    sleep(20);

    return 0;
}





