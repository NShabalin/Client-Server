#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <data_structures/message.h>
#include <thread>
#include <openssl/ssl.h>
#include <openssl/err.h>


class Client {
private:
    SSL_CTX* sslCtx = nullptr;
    SSL* sslConnection = nullptr;
    const char* caBundlePath = "client/ca_bundle.pem";

    int clientSocket;
    sockaddr_in serverAddress;
    std::string logsPath;
    uint64_t messageId;
    
public:
    Client() {
        clientSocket = -1;
        messageId = 0;
        logsPath = "client/logs/client_" + get_current_time() + ".txt";
        init();
    }

    void init() {
        log("Started client initialization");
        // wsa запуск для виндовс
        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            log("WSA startup failed");
            return;
        }
        #endif

        // подключение к серверу
        while (!tls_connect()) {
            log("Couldn't connect to the server. Trying to reconnect.");
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        log("Initialization complete");
    }

    bool socket_init() {
        if (clientSocket != -1) {
            #ifdef _WIN32
            closesocket(clientSocket);
            #else
            close(clientSocket);
            #endif
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket < 0) {
            log("Socket initialization failed");
            return false;
        }
        // структура сервера
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_port = htons(7111);
        int result = inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
        if (result <= 0) {
            log("ip address not valid");
            return false;
        }
        return true;
    }

    bool tls_connect() {
        log("Initializing the socket");
        socket_init();

        log("Connecting to the server over TLS");

        // подключение к серверу через TCP сокет
        if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            log("Connection to the server over TCP socket failed");
            return false;
        }

        // Инициализация контекста TLS
        sslCtx = SSL_CTX_new(TLS_client_method());
        if (!sslCtx) { 
            log("Initializing TLS context failed:" + openssl_last_error());
            return false; 
        }

        // Верификация сервера
        if (SSL_CTX_load_verify_locations(sslCtx, caBundlePath, nullptr) != 1) {
            log("Server verification failed: " + openssl_last_error());
            return false;
        }

        // привязка TLS объекта к сокету
        sslConnection = SSL_new(sslCtx);
        SSL_set_fd(sslConnection, clientSocket);

        // Соединение к серверу по TLS
        if (SSL_connect(sslConnection) <= 0) {
            log("TLS connection failed: " + openssl_last_error());
            SSL_free(sslConnection);  
            sslConnection = nullptr;
            return false;
        }

        // конвертирование адреса клиента в строку
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &serverAddress.sin_addr, ipStr, INET_ADDRSTRLEN);
        unsigned short serverPort = ntohs(serverAddress.sin_port);
        std::string serverAddr = std::string(ipStr) + ":" + std::to_string(serverPort);

        log("Connection complete. Address: " + serverAddr);
        return true;
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
    
    bool send_message(Message message) {
        log("Sending message with id " + std::to_string(message.getId()));

        nlohmann::json messageJson;
        messageJson["id"] = message.getId();
        messageJson["size"] = message.getSize();
        messageJson["data"] = message.getData();

        std::string text = messageJson.dump();
        
        // передача длины сообщения
        uint64_t length = htobe64(text.size());
        int sendResult = SSL_write(sslConnection, &length, sizeof length);
        if (sendResult < 0) {
            std::cout << "Sending message failed\n";
            log("Sending message failed:" + openssl_last_error());
            return false;
        }

        // передача сообщения через сокет
        sendResult = SSL_write(sslConnection, text.c_str(), text.size());
        if (sendResult < 0) {
            std::cout << "Sending message failed\n";
            log("Sending message failed:" + openssl_last_error());
            return false;
        }
        else {
            log("Message sent successfuly");
            return true;
        }
    }


    // TODO: send error with the message
    Message receive_message() {
        log("Receiving message");

        // баффер для получения сообщения
        std::vector<char> buffer(8);
        memset(buffer.data(), 0, buffer.size());

        int bytesToReceive = 8;
        int bytesReceived = 0;

        // получение первых 8 байт для определения длины сообщения
        while (bytesReceived < bytesToReceive) {
            int received = SSL_read(sslConnection, buffer.data() + bytesReceived, bytesToReceive - bytesReceived);
            if (received <= 0) {
                log("Error while getting the message: " + openssl_last_error());
                return Message(0, 0, nlohmann::json::object());
            }
            bytesReceived += received;
        }

        uint64_t messageLength = be64toh(*(uint64_t*)(buffer.data()));
        if (messageLength <= 0) {
            log("Error, the message length is 0");
            return Message(0, 0, nlohmann::json::object());
        }
        if (messageLength > 10 * 1024 * 1024) {
            log("Error, the message length is more than the maximum");
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
                log("Error while getting the message");
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
        log("Message with id " + std::to_string(message.getId()) + " received successfully");
        return message;
    }

    Message send_command(std::string command, std::vector<std::string> args = {}) {
        if (command != "get_all_tables" &&
            command != "get_table_data" &&
            command != "change_table_entry" &&
            command != "check_json_integrity") {
            log("The command \"" + command + "\" does not exist");
            return Message(0, 0, nlohmann::json::object());
        }
        // создание сткуртуры Message для передачи команды
        messageId++;
        uint64_t size = command.capacity();
        nlohmann::json commandJson;
        commandJson["command"] = command;
        // добавление аргументов команды, если они есть
        if (!args.empty()) {
            commandJson["args"];
            for (std::string arg : args) {
                commandJson["args"].push_back(arg);
            }
        }
        // отправка сообщения серверу
        log("Sending a command " + command);
        Message message(messageId, size, commandJson);
        send_message(message);
        // получение ответа от сервера
        Message receivedMessage = receive_message();

        // если произошла ошибка
        while (receivedMessage.getId() == 0) {
            std::cout << "Ошибка во время получения сообщения. Попытка переприсоединения к серверу.";
            log("Error while getting the message. Checking the socket and trying to reconnect to the server.");

            // проверка сокета
            if (!check_client_socket()) {
                log("Trying to reconnect.");
            }

            if (tls_connect()) {
                break;
            }
        }

        return receivedMessage;
    }

    bool check_client_socket() {
        #ifdef _WIN32
        // в WinSock нет MSG_DONTWAIT, делаем временно неблокирующим сокет
        u_long nonblock = 1;
        ioctlsocket(clientSocket, FIONBIO, &nonblock);

        char tmp;
        // просматриваем один байт
        int n = recv(clientSocket, &tmp, 1, MSG_PEEK);

        // возвращаем блокирующий режим
        nonblock = 0;
        ioctlsocket(clientSocket, FIONBIO, &nonblock);

        if (n > 0) 
            return true;
        if (n == 0) 
            return false;

        int err = WSAGetLastError();
        // нет данных, но сокет впорядке
        if (err == WSAEWOULDBLOCK) 
            return true;
        return false;

        #else 
        
        char tmp;
        int n = recv(clientSocket, &tmp, 1, MSG_DONTWAIT | MSG_PEEK);
        if (n > 0) 
            return true;
        if (n == 0) 
            return false;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        return false;  
        #endif
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
    
    void WSA_cleanup() {
        #ifdef _WIN32
        WSACleanup();
        #endif
    }
};

