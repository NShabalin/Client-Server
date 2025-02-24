#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
// includes for linux
#endif

#include <cstdlib>
#include <iostream>

class Server {
private:
    int serverSocket;
    sockaddr_in serverAddress;

    int clientSocket;
    sockaddr_in clientAddress;
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
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket < 0) {
            std::cout << "Socket initialization failed\n";
            #ifdef _WIN32
            WSACleanup();
            #endif
            return;
        }

        // structure
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_addr.s_addr = INADDR_ANY; // any ip address
        serverAddress.sin_port = htons(7111);

        // binding the socket to the ip address
        if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cout << "Socket binding failed\n";
            #ifdef _WIN32
            closesocket(serverSocket);
            WSACleanup();
            #else
            close(serverSocket);
            #endif
            return;
        }

        // socket to listening mode
        if (listen(serverSocket, SOMAXCONN) < 0) {
            std::cout << "Socket to listening failed\n";
            #ifdef _WIN32
            closesocket(serverSocket);
            WSACleanup();
            #else
            close(serverSocket);
            #endif
            return;
        }

        std::cout << "Sever initialization successful\n";
        wait_connection();
    }

    void wait_connection() {
        socklen_t clientSize = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientSize);
        if (clientSocket < 0) {
            std::cout << "Connection accept failed\n";
            #ifdef _WIN32
            closesocket(serverSocket);
            WSACleanup();
            #else
            close(serverSocket);
            #endif
            return;
        }
        std::cout << "Connection successful\n";

        recieve_data();
    }

    void recieve_data() {
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));

        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cout << "Data receive failed\n";
            return;
        }

        std::cout << "Recieved from client: " << std::string(buffer, bytesReceived) << "\n";

        send_data();
    }

    void send_data() { 
        std::string message = "Message from server!";
        int sendResult = send(clientSocket, message.c_str(), message.size(), 0);
        if (sendResult < 0) {
            std::cout << "Sending message failed\n";
        }
    }

    void sever_close() {
        #ifdef _WIN32
        WSACleanup();
        closesocket(serverSocket);
        #else
        close(serverSocket);
        #endif
    }
};