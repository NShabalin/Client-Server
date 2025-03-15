#pragma once

#include <include/nlohmann/json.hpp>
#include <stack>
#include <filesystem>
#include <iostream>
#include <fstream>

class ServerCommands {
private:
    std::filesystem::path rootPath = "server/tables";
public:
    ServerCommands() {
    }

    nlohmann::json do_command(nlohmann::json commandJson) {
        nlohmann::json result;
        // название команды
        std::string command = commandJson["command"];
        // аргументы команды, если они есть
        std::vector<std::string> args;
        if (!commandJson["args"].empty()) {
            nlohmann::json argsJson = commandJson["args"];
            args = argsJson.get<std::vector<std::string>>();
        }

        if (command == "get_all_tables") {
            result = get_all_tables();
        }
        if (command == "get_table_data") {
            result = get_table_data(args);
        }
        if (command == "change_table_entry") {
            result = change_table_entry(args);
        }

        return result;
    }

    // получения списка всех таблиц
    nlohmann::json get_all_tables() {
        nlohmann::json fileTree;        
        // стек из пути к дикертории и ссылки на его позицию в json-объекте 
        std::stack<std::pair<std::filesystem::path, nlohmann::json*>> directories;
        directories.push({rootPath, &fileTree});

        while (!directories.empty()) {
            auto [currentDirectory, currentJson] = directories.top();
            directories.pop();
            // для всех файлов в папке
            for (auto& entry : std::filesystem::directory_iterator(currentDirectory)) {
                std::string entryPath = entry.path().filename().string();
                // если это папка
                if (entry.is_directory()) {
                    // создается папка в текущей папке, добавляется в стек
                    (*currentJson)[entryPath] = nlohmann::json::array();
                    directories.push({entry.path(), &(*currentJson)[entryPath]});
                }
                else {
                    // если файл, просто добавляется в текущую папку
                    currentJson->push_back(entryPath);
                }
            }
        }

        return fileTree;
    }

    // получение всех данных из таблицы
    nlohmann::json get_table_data(std::vector<std::string> args) {
        std::string tableName = args[0];
        // создается json-элемет из файла
        std::ifstream ifs(rootPath.string() + "/" + tableName + ".json");
        nlohmann::json tableJson = nlohmann::json::parse(ifs);

        return tableJson;
    }

    // изменение клетки в таблице
    nlohmann::json change_table_entry(std::vector<std::string> args) {
        std::string tableName = args[0];
        int rowNumber = std::stoi(args[1]);
        std::string colName = args[2];
        std::string newValue = args[3];
        // получение json-элемента таблицы
        nlohmann::json tableJson = get_table_data(args);
        // получение строки
        nlohmann::json& rowJson = tableJson[rowNumber];
        // изменение клетки на новое значение
        rowJson[colName] = newValue;

        // запись нового объекта в файл
        std::ofstream ofs(rootPath.string() + "/" + tableName + ".json");
        ofs << tableJson.dump(4);
        ofs.close();

        return tableJson;
    }
};