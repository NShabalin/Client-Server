#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
// includes for linux
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
#include <encryptor_OpenSSL.h>

class Server {
private:
    int serverSocket;
    sockaddr_in serverAddress;

    std::string logsPath;
    uint64_t messageId;
public:
    Server() {
    }

    void start() {
        logsPath = "server/logs/server_" + get_current_time() + ".txt";
        messageId = 0;
        serverSocket = -1;
        init();

        while(true) {
            int clientSocket = -1;
            sockaddr_in clientAddress;

            // ожидание подключения клиента
            while (!wait_connection(clientSocket, clientAddress)) {
                if (!check_server_socket()) {
                    if (!socket_init()) {
                        log("Couldn't reinitialize the server socket. Exiting the program.");
                        exit(0);
                    }
                }
            }
            // создание нового потока для каждого клиента для обработки нескольких клиентов сразу
            std::thread clientThread([this, clientSocket, clientAddress]() {
                this->client_thread(clientSocket, clientAddress);
            });
            clientThread.detach();
        }
    }

    void client_thread(int clientSocket, sockaddr_in clientAddress) {
        // конвертирование адреса клиента в строку
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, ipStr, INET_ADDRSTRLEN);
        unsigned short clientPort = ntohs(clientAddress.sin_port);
        std::string clientAddr = std::string(ipStr) + ":" + std::to_string(clientPort);

        std::string logsEndline = " (Client address: " + clientAddr + ")";

        while (true) {
            log("Waiting for a command." + logsEndline);
            Message message = receive_message(clientSocket, logsEndline);
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

            if (!send_message(answer, clientSocket, logsEndline)) {
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

        return true;
    }

    bool wait_connection(int& clientSocket, sockaddr_in& clientAddress) {
        // если сокет уже существует, он закрывается для переподключения
        if (clientSocket != -1) {
            log("Closing client socket");
            close_socket(clientSocket);
        }
        log("Waiting for client connection");
        socklen_t clientSize = sizeof(clientAddress);
        // ожидание подключения
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientSize);
        // ошибка подключения
        if (clientSocket < 0) {
            log("Connection accept failed");
            return false;
        }
        else {
            log("Connection to a client successful");
            return true;
        }
    }

    bool send_message(Message message, int clientSocket, std::string logsEndline) {
        log("Sending message with id " + std::to_string(message.getId()) + "." + logsEndline);

        nlohmann::json messageJson;
        messageJson["id"] = message.getId();
        messageJson["size"] = message.getSize();
        messageJson["data"] = message.getData();
        
        // шифровка сообщения
        std::string text = messageJson.dump();
        std::vector<char> textEncrypted;
        encryptorOpenSSL enc(logsPath, logsEndline);
        if (!enc.encrypt(text, textEncrypted)) {
            log("Encryption failed." + logsEndline);
            return false;
        }

        // передача сообщения через сокет
        int sendResult = send(clientSocket, textEncrypted.data(), textEncrypted.size(), 0);
        // если произошла обшибка
        if (sendResult < 0) {
            log("Sending message failed." + logsEndline);
            return false;
        }
        else {
            log("Message sent successfuly." + logsEndline);
            return true;
        }
    }

    Message receive_message(int clientSocket, std::string logsEndline) {
        log("Receiving message." + logsEndline);
        // баффер для получения сообщения, до 10мб
        std::vector<char> textEncrypted(10 * 1024 * 1024);
        memset(textEncrypted.data(), 0, textEncrypted.size());

        // получение сообщения через сокет
        int bytesReceived = recv(clientSocket, textEncrypted.data(), textEncrypted.size(), 0);
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
            log("Error while getting the message." + logsEndline);
            return Message(0, 0, nlohmann::json::object());
        }
        else {
            textEncrypted.resize(bytesReceived);
            std::string text;
            encryptorOpenSSL enc(logsPath, logsEndline);
            if (!enc.decrypt(text, textEncrypted)) {
                log("Decrypting the message failed." + logsEndline);
                return Message(0, 0, nlohmann::json::object());
            }

            // конвертация сообщения из баффера в структуру Message
            nlohmann::json messageJson = nlohmann::json::parse(text);
            Message message(messageJson["id"], messageJson["size"], messageJson["data"]);
            if (message.getId() > messageId) {
                messageId = message.getId();
            }
            log("Message with id " + std::to_string(message.getId()) + " received successfully." + logsEndline);
            return message;
        }
        return Message(0, 0, nlohmann::json::object());
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
};