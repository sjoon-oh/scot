#include <unistd.h>
#include <iostream>

#include "../includes/scot-core.hh"

int main() {

    scot::ScotCore scot_instance;
    scot_instance.initialize();

    int nid = scot::ScotConnection::get_instance().get_my_nid();

    sleep(3);

    if (nid == 0) {
        
        const char* static_buffer = "123456781234567812345678";
        scot_instance.propose(
            (uint8_t*)static_buffer, 16, (uint8_t*)static_buffer, 4, 1234);
        
    }
    else {
        sleep(10);
    }

    sleep(7);
    scot_instance.finish();
    
    std::cout << "Test end. \n";

    return 0;
}