#pragma once

#include <iostream>
#include <server/server.h>

void run_server() {
    Server server;
    std::thread serverThread([&server](){
        server.start();
    });
    serverThread.detach();

    while (true) {
        std::cout << "Введите \"exit\" для завершения работы сервера.\n";
        std::string input;
        std::cin >> input;

        if (input == "exit") {
            server.server_close();
            exit(0);
        }
    }
}
