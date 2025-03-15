#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
// includes for linux
#endif

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <data_structures/message.h>
#include <thread>


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
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket < 0) {
            log("Socket initialization failed");
            #ifdef _WIN32
            WSACleanup();
            #endif
            return;
        }
        // структура сервера
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET; // ipv4
        serverAddress.sin_port = htons(7111);
        int result = inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
        if (result <= 0) {
            log("ip address not valid");
            #ifdef _WIN32
            closesocket(clientSocket);
            WSACleanup();
            #else
            close(clientSocket);
            #endif
            return;
        }
    
        log("Initialization complete");
        // подключение к серверу
        while (!connect_to_sever()) {
            log("Couldn't connect to the server. Trying again in 5 seconds.");
            std::this_thread::sleep_for(std::chrono::seconds(5));
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

    bool connect_to_sever() {
        log("Connecting to the sever");
        // попытка подключения к серверу
        if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            log("Connection failed");
            return false;
        }
        log("Connection complete");
        return true;
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

    Message send_command(std::string command, std::vector<std::string> args = {}) {
        if (command != "get_all_tables" &&
            command != "get_table_data" &&
            command != "change_table_entry") {
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
            log("Error while getting the message. Trying to reconnect to the server.");
            if (connect_to_sever()) {
                receivedMessage = receive_message();
            }
            else {
                log("Couldn't connect to the server. Trying again in 5 seconds.");
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }

        return receivedMessage;
    }
};