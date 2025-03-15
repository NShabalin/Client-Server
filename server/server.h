#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
// includes for linux
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
    int serverSocket;
    sockaddr_in serverAddress;

    int clientSocket;
    sockaddr_in clientAddress;

    std::string logsPath;
    uint64_t messageId;
public:
    Server() {
        logsPath = "server/logs/server_" + get_current_time() + ".txt";
        messageId = 0;
        clientSocket = -1;
        serverSocket = -1;
        init();

        // постоянно ожидает получение команды от клиента
        while(true) {
            log("Waiting for a command");
            Message message = receive_message();
            bool check;
            // если произошла ошибка при получении команды
            while (message.getId() == 0) {
                log("Error occured while getting the message. Trying to recconect to the client.");
                // попытка переподключиться каждые 5 секунд                
                if (wait_connection()) {
                    message = receive_message();
                }
                else {
                    log("Reconnection failed. Trying again in 5 seconds.");
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
            // передача данных команды на выполнение
            ServerCommands serverC;
            log("Executing the command: " + message.getData().dump());
            nlohmann::json commandOutput = serverC.do_command(message.getData());
            log("Command executed");
            // команда исполнена, передача ответа клиенту
            messageId++;
            uint64_t size = commandOutput.dump().capacity();
            Message answer(messageId, size, commandOutput);

            send_message(answer);
        }
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
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket < 0) {
            log("Socket initialization failed");
            #ifdef _WIN32
            WSACleanup();
            #endif
            return;
        }
        // структура сервера
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_addr.s_addr = INADDR_ANY; // любой ip-адрес
        serverAddress.sin_port = htons(7111);
        // связывание сокета с ip-адресом
        if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            log("Socket binding failed");
            server_close();
        }
        // прослушивающий режим сокета
        if (listen(serverSocket, SOMAXCONN) < 0) {
            log("Socket to listening failed");
            server_close();
        }

        log("Server initialization successful");
        std::cout << "Server initialization successful\n";
        // подключение с клиентом
        while (!wait_connection()) {
            log("Couldn't connect to the client. Trying again in 5 seconds.");
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    bool wait_connection() {
        // если сокет уже существует, он закрывается для переподключения
        if (clientSocket != -1) {
            log("Closing current socket");
            #ifdef _WIN32
            closesocket(clientSocket);
            #else
            close(clientSocket);
            #endif
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
            log("Connection to client successful");
            std::cout << "Connection to client successful\n";
            return true;
        }
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

    bool send_message(Message message) {
        log("Sending message with id " + std::to_string(message.getId()));

        nlohmann::json messageJson;
        messageJson["id"] = message.getId();
        messageJson["size"] = message.getSize();
        messageJson["data"] = message.getData();
        // передача сообщения через сокет
        int sendResult = send(clientSocket, messageJson.dump().c_str(), messageJson.dump().size(), 0);
        // если произошла обшибка
        if (sendResult < 0) {
            std::cout << "Sending message failed\n";
            log("Sending message failed");
            return false;
        }
        else {
            log("Message sent successfuly");
            return true;
        }
    }

    Message receive_message() {
        log("Receiving message");
        // баффер для получения сообщения, до 10мб
        std::vector<char> buffer(10 * 1024 * 1024);
        memset(buffer.data(), 0, buffer.size());
        // получение сообщения через сокет
        int bytesReceived = recv(clientSocket, buffer.data(), buffer.size(), 0);
        // если произошла ошибка
        if (bytesReceived <= 0) {
            log("No response while getting the message");
            return Message(0, 0, nlohmann::json::object());
        }
        else {
            // конвертация сообщения из баффера в структуру Message
            std::string strMessage = std::string(buffer.data(), bytesReceived);
            nlohmann::json messageJson = nlohmann::json::parse(strMessage);
            Message message(messageJson["id"], messageJson["size"], messageJson["data"]);
            if (message.getId() > messageId) {
                messageId = message.getId();
            }
            log("Message with id " + std::to_string(message.getId()) + " received successfully");
            return message;
        }
        return Message(0, 0, nlohmann::json::object());
    }

    void server_close() {
        #ifdef _WIN32
        WSACleanup();
        closesocket(serverSocket);
        #else
        close(serverSocket);
        #endif
        exit(0);
    }

    
};