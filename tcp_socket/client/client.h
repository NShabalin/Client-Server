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
#include <encryptor_OpenSSL.h>


class Client {
private:
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
        // запуск сокета
        if (!socket_init()) {
            close_socket();
            log("Error while initializing the socket. Exiting the program.");
            exit(0);
        }
    
        log("Initialization complete");
        // подключение к серверу
        while (!connect_to_sever()) {
            log("Couldn't connect to the server. Trying to reconnect.");
            std::cout << "Ошибка соединения с сервером. Попытка переподключения.\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    bool socket_init() {
        if (clientSocket != -1) {
            close_socket();
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket < 0) {
            log("Socket initialization failed");
            close_socket();
            return false;
        }
        // структура сервера
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_port = htons(7111);
        int result = inet_pton(AF_INET, "192.168.56.101", &serverAddress.sin_addr);
        if (result <= 0) {
            log("ip address not valid");
            return false;
        }
        return true;
    }

    bool connect_to_sever() {
        log("Connecting to the sever");
        // попытка подключения к серверу
        if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            log("Connection failed");
            socket_init();
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
    
    bool send_message(Message message) {
        reconnecting();

        log("Sending message with id " + std::to_string(message.getId()));

        nlohmann::json messageJson;
        messageJson["id"] = message.getId();
        messageJson["size"] = message.getSize();
        messageJson["data"] = message.getData();

        // шифровка сообщения
        std::string text = messageJson.dump();
        std::vector<char> textEncrypted;
        encryptorOpenSSL enc(logsPath, "\n");
        if (!enc.encrypt(text, textEncrypted)) {
            log("Encryption failed.");
            return false;
        }

        // передача сообщения через сокет
        int sendResult = send(clientSocket, textEncrypted.data(), textEncrypted.size(), 0);
        // если произошла обшибка
        if (sendResult < 0) {
            std::cout << "Ошибка отправки сообщения\n";
            log("Sending message failed");
            return false;
        }
        else {
            log("Message sent successfully");
            return true;
        }
    }

    Message receive_message() {
        log("Receiving message");
        // баффер для получения сообщения, до 10мб
        std::vector<char> textEncrypted(10 * 1024 * 1024);
        memset(textEncrypted.data(), 0, textEncrypted.size());

        // получение сообщения через сокет
        int bytesReceived = 0;
        bool error = false;
        // используется select для случев, когда сообщение передается несколькими пакетами
        while (true) {
            // сет для чтения
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(clientSocket, &readfds);
            // таймаут селекта в пол секунды
            struct timeval timeout = { 0, 500000 };

            int selectResult = select(clientSocket + 1, &readfds, NULL, NULL, &timeout);
            if (selectResult > 0 && FD_ISSET(clientSocket, &readfds)) {
                int received = recv(clientSocket, textEncrypted.data() + bytesReceived, textEncrypted.size() - bytesReceived, 0);

                if (received <= 0) {
                    error = true;
                    break;
                }
                bytesReceived += received;
            }
            else {
                break;
            }
        }
        
        // если произошла ошибка
        if (bytesReceived <= 0 || error) {
            log("Error while getting the message");
            return Message(0, 0, nlohmann::json::object());
        }
        else {
            textEncrypted.resize(bytesReceived);
            std::string text;
            encryptorOpenSSL enc(logsPath, "\n");
            if (!enc.decrypt(text, textEncrypted)) {
                log("Decrypting the message failed.");
                return Message(0, 0, nlohmann::json::object());
            }

            // конвертация сообщения из баффера в структуру Message
            nlohmann::json messageJson = nlohmann::json::parse(text);
            Message message(messageJson["id"], messageJson["size"], messageJson["data"]);
            if (message.getId() > messageId) {
                messageId = message.getId();
            }
            log("Message with id " + std::to_string(message.getId()) + " received successfully");
            return message;
        }
        return Message(0, 0, nlohmann::json::object());
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
        
        if (!send_message(message)) {
            return Message(0, 0, nlohmann::json::object());
        }

        // получение ответа от сервера
        Message receivedMessage = receive_message();
        // если произошла ошибка
        while (receivedMessage.getId() == 0) {
            log("Error while getting the message. Checking the socket and trying to reconnect to the server.");

            // проверка сокета
            reconnecting();

            if (connect_to_sever()) {
                receivedMessage = receive_message();
            }
            else {
                log("Couldn't connect to the server.");
                return Message(0, 0, nlohmann::json::object());
            }
        }

        return receivedMessage;
    }

    bool check_client_socket() {
<<<<<<< HEAD
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
=======
        int error = 0;
        socklen_t length = sizeof(error);
        #ifdef _WIN32
        if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) < 0) {
            log("Error discovered while getting the socket options:" + std::string(strerror(error)));
            return false;
        }
        #else
        if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
            log("Error discovered while getting the socket options:" + std::string(strerror(error)));
            return false;
        }
        #endif
        if (error != 0) {
            log("Error discovered while checking the socket:" + std::string(strerror(error)));
        }
        return true;
>>>>>>> 00f3038686d47f20677eadeaf87b7afb6c1f00f8
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

    void reconnecting() {
        if (!check_client_socket()) {
            std::cout << "Ошибка соединения с сервером. Попытка переподключения.\n";
            while (!connect_to_sever()) {
                log("Couldn't connect to the server. Trying to reconnect.");
                std::cout << "Ошибка соединения с сервером. Попытка переподключения.\n";
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }

    void close_socket() {
        #ifdef _WIN32
        closesocket(clientSocket);
        WSACleanup();
        #else
        close(clientSocket);
        #endif
    }
};

