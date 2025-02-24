#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
// includes for linux
#endif

#include <cstdlib>
#include <iostream>

class Client {
private:
    int clientSocket;
    
    sockaddr_in serverAddress;
public:
    void init() {
        // wsa startup for windows
        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            std::cout << "WSA startup failed\n";
            return;
        }
        #endif

        // initializing the socket
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket < 0) {
            std::cout << "Socket initialization failed\n";
            #ifdef _WIN32
            WSACleanup();
            #endif
            return;
        }

        // structure
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_port = htons(7111);
        int result = inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
        if (result <= 0) {
            std::cout << "ip address not valid\n";
            #ifdef _WIN32
            closesocket(clientSocket);
            WSACleanup();
            #else
            close(clientSocket);
            #endif
            return;
        }

        std::cout << "Initialization complete\n";
        connect_to_sever();
    }

    void connect_to_sever() {
        if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cout << "Connection failed\n";
            #ifdef _WIN32
                closesocket(clientSocket);
                WSACleanup();
            #else
                close(clientSocket);
            #endif
            return;
        }

        send_message();
    }

    void send_message() {
        std::string message = "Message from client!";
        int sendResult = send(clientSocket, message.c_str(), message.size(), 0);
        if (sendResult < 0) {
            std::cout << "sending message failed\n";
        }
        else {
            std::cout << "Sended message\n";
            recieve_answer();
        }

    }

    void recieve_answer() {
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::cout << "Server responded: " << std::string(buffer, bytesReceived) << "\n";
        }
        else {
            std::cout << "No server response\n";
        }
    }
};