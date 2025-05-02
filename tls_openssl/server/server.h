#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <cstdlib>
#include <iostream>
#include <data_structures/message.h>
#include <server/server_commands.h>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>

class Server {
private:
    SSL_CTX* sslCtx = nullptr;
    const char* certificatePath = "server/server.crt";
    const char* keyPath = "server/server.key";

    int serverSocket;
    sockaddr_in serverAddress;

    std::string logsPath = "server/logs/server_" + get_current_time() + ".txt";
    uint64_t messageId;
public:
    Server() {
    }

    void start() {
        messageId = 0;
        serverSocket = -1;
        init();

        while(true) {
            int clientSocket = -1;
            sockaddr_in clientAddress;
            SSL *sslConnection = nullptr;

            // ожидание подключения клиента
            while (!wait_connection(clientSocket, clientAddress, sslConnection)) {
                if (!check_server_socket()) {
                    if (!socket_init()) {
                        log("Couldn't reinitialize the server socket. Exiting the program.");
                        exit(0);
                    }
                }
            }
            // создание нового потока для каждого клиента для обработки нескольких клиентов сразу
            std::thread clientThread([this, clientSocket, clientAddress, sslConnection]() {
                this->client_thread(clientSocket, clientAddress, sslConnection);
            });
            clientThread.detach();
        }
    }

    void client_thread(int clientSocket, sockaddr_in clientAddress, SSL* sslConnection) {
        // конвертирование адреса клиента в строку
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, ipStr, INET_ADDRSTRLEN);
        unsigned short clientPort = ntohs(clientAddress.sin_port);
        std::string clientAddr = std::string(ipStr) + ":" + std::to_string(clientPort);

        std::string logsEndline = " (Client address: " + clientAddr + ")";

        while (true) {
            log("Waiting for a command." + logsEndline);
            Message message = receive_message(clientSocket, sslConnection, logsEndline);
            // если произошла ошибка при получении команды
            if (message.getId() == 0) {
                log("Error occured while getting the message from the client. Closing the connection with the client." + logsEndline);
                close_socket(clientSocket);
                break;
            }

            // передача данных команды на выполнение
            ServerCommands serverC(logsPath, logsEndline);
            log("Executing the command: " + message.getData().dump() + "." + logsEndline);
            nlohmann::json commandOutput = serverC.do_command(message.getData());
            if (commandOutput.is_null()) {
                log ("Error while executing the command." + logsEndline);
            }
            else log("Command executed." + logsEndline);
            // команда исполнена, передача ответа клиенту
            messageId++;
            uint64_t size = commandOutput.dump().capacity();
            Message answer(messageId, size, commandOutput);

            if (!send_message(answer, clientSocket, sslConnection, logsEndline)) {
                log("Error occured while sending the message to the client. Closing the connection with the client." + logsEndline);
                close_socket(clientSocket);
                break;
            }
        }
        close_socket(clientSocket);
    }

    void init() {
        log("Initializing server");
        // wsa запуск для виндовс
        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            log("WSA startup failed");
            return;
        }
        #endif

        // запуск сокета
        if (!socket_init()) {
            log("Error while initializing the socket. Exiting the program.");
            server_close();
        }

        log("Server initialization successful");
        std::cout << "Сервер успешно запущен\n";
    }

    bool socket_init() {
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket < 0) {
            log("Socket initialization failed");
            return false;
        }
        // структура сервера
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_addr.s_addr = INADDR_ANY; // любой ip-адрес
        serverAddress.sin_port = htons(7111);
        // связывание сокета с ip-адресом
        if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            log("Socket binding failed");
            return false;
        }
        // прослушивающий режим сокета
        if (listen(serverSocket, SOMAXCONN) < 0) {
            log("Socket to listening failed");
            return false;
        }

        // создание TLS контекста
        sslCtx = SSL_CTX_new(TLS_server_method());
        if (!sslCtx) { 
            log("Error creating TLS context: " + openssl_last_error()); 
            return false;
        }
        if (SSL_CTX_use_certificate_file(sslCtx, certificatePath, SSL_FILETYPE_PEM) <= 0 || 
            SSL_CTX_use_PrivateKey_file(sslCtx, keyPath, SSL_FILETYPE_PEM) <= 0 || 
            !SSL_CTX_check_private_key(sslCtx)) {

            log("Error creating TLS context: " + openssl_last_error());  
            return false;
        }

        return true;
    }

    bool wait_connection(int& clientSocket, sockaddr_in& clientAddress, SSL*& sslConnection) {
        // если сокет уже существует, он закрывается для переподключения
        if (clientSocket != -1) {
            log("Closing client socket");
            close_socket(clientSocket);
        }

        log("Waiting for client connection");
        socklen_t clientSize = sizeof(clientAddress);

        // ожидание подключения
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientSize);
        if (clientSocket < 0) {
            log("Connection accept failed");
            return false;
        }

        // привязка TLS к сокету
        sslConnection = SSL_new(sslCtx);
        SSL_set_fd(sslConnection, clientSocket);

        // Соединение с клиентом по TLS
        if (SSL_accept(sslConnection) <= 0) {
            log("Error while connecting to the client: " + openssl_last_error());
            close_socket(clientSocket);
            SSL_free(sslConnection);
            sslConnection = nullptr;
            return false;
        }

        log("Connection to a client successful");
        return true;
    }

    bool send_message(Message message, int clientSocket, SSL* sslConnection, std::string logsEndline) {
        log("Sending message with id " + std::to_string(message.getId()) + "." + logsEndline);

        nlohmann::json messageJson;
        messageJson["id"] = message.getId();
        messageJson["size"] = message.getSize();
        messageJson["data"] = message.getData();
        
        std::string text = messageJson.dump();

        /// передача длины сообщения
        uint64_t length = htobe64(text.size());
        int sendResult = SSL_write(sslConnection, &length, sizeof length);
        if (sendResult < 0) {
            std::cout << "Sending message failed\n";
            log("Sending message failed:" + openssl_last_error() + logsEndline);
            return false;
        }

        // передача сообщения через сокет
        sendResult = SSL_write(sslConnection, text.c_str(), text.size());
        if (sendResult < 0) {
            std::cout << "Sending message failed\n";
            log("Sending message failed:" + openssl_last_error() + logsEndline);
            return false;
        }
        else {
            log("Message sent successfuly" + logsEndline);
            return true;
        }
    }

    Message receive_message(int clientSocket, SSL* sslConnection, std::string logsEndline) {
        log("Receiving message." + logsEndline);
        
        // баффер для получения сообщения
        std::vector<char> buffer(8);
        memset(buffer.data(), 0, buffer.size());

        int bytesToReceive = 8;
        int bytesReceived = 0;

        // получение первых 8 байт для определения длины сообщения
        while (bytesReceived < bytesToReceive) {
            int received = SSL_read(sslConnection, buffer.data() + bytesReceived, bytesToReceive - bytesReceived);
            if (received <= 0) {
                log("Error while getting the message: " + openssl_last_error() + logsEndline);
                return Message(0, 0, nlohmann::json::object());
            }
            bytesReceived += received;
        }

        uint64_t messageLength = be64toh(*(uint64_t*)(buffer.data()));
        if (messageLength <= 0) {
            log("Error, the message length is 0" + logsEndline);
            return Message(0, 0, nlohmann::json::object());
        }
        if (messageLength > 10 * 1024 * 1024) {
            log("Error, the message length is more than the maximum" + logsEndline);
            return Message(0, 0, nlohmann::json::object());
        }

        bytesToReceive = messageLength;
        bytesReceived = 0;
        buffer.resize(bytesToReceive);
        memset(buffer.data(), 0, buffer.size());
        
        // получение сообщения
        while (bytesReceived < bytesToReceive) {
            int received = SSL_read(sslConnection, buffer.data() + bytesReceived, bytesToReceive - bytesReceived);
            if (received <= 0) {
                log("Error while getting the message: " + openssl_last_error() + logsEndline);
                return Message(0, 0, nlohmann::json::object());
            }
            bytesReceived += received;
        }

        // конвертация сообщения из баффера в структуру Message
        nlohmann::json messageJson = nlohmann::json::parse(buffer);
        Message message(messageJson["id"], messageJson["size"], messageJson["data"]);
        if (message.getId() > messageId) {
            messageId = message.getId();
        }
        log("Message with id " + std::to_string(message.getId()) + " received successfully" + logsEndline);
        return message;
    }

    void server_close() {
        log("Closing the server.");
        #ifdef _WIN32
        WSACleanup();
        closesocket(serverSocket);
        #else
        close(serverSocket);
        #endif
        exit(0);
    }

    void close_socket(int socket) {
        #ifdef _WIN32
        closesocket(socket);
        #else
        close(socket);
        #endif
    }

    std::string openssl_last_error() {
        unsigned long err = ERR_get_error();
        if (err == 0) { 
            return "No OpenSSL error";
        }

        char buff[256];
        ERR_error_string_n(err, buff, sizeof(buff));
        return std::string(buff);
    }

    bool check_server_socket() {
        int error = 0;
        socklen_t length = sizeof(error);
        #ifdef _WIN32
        if (getsockopt(serverSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) < 0) {
            log("Error discovered while getting the socket options:" + std::string(strerror(error)));
            return false;
        }
        #else
        if (getsockopt(serverSocket, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
            log("Error discovered while getting the socket options:" + std::string(strerror(error)));
            return false;
        }
        #endif
        if (error != 0) {
            log("Error discovered while checking the socket:" + std::string(strerror(error)));
        }
        return true;
    }

    std::string get_current_time() {
        // текущее время
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        // конвертация в локальное время
        std::tm local_time = *std::localtime(&now_time);
        // в формат "DD-MM-YYYY_HH-MM-SS"
        std::ostringstream oss;
        oss << std::put_time(&local_time, "%d-%m-%Y_%H-%M-%S");

        return oss.str();
    }

    void log(const std::string message) {
        std::ofstream logFile(logsPath, std::ios_base::app);
        if (logFile.is_open()) {
            logFile << get_current_time() << ": " << message << "\n";
        }
    }
};