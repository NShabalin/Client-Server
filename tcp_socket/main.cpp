#include <iostream>
#include <limits>
#include <client/main_client.h>
#include <server/main_server.h>

int main() {
    std::cout << "1 - запустить клиент\n2 - запустить сервер\n0 - выход\n";
    int input = get_int_input(0, 2);

    switch (input) {
        case 1:
            run_client();
            break;
        case 2:
            run_server();
            break;
        case 0:
            break;
    }
    
    return 0;
}