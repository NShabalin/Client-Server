#pragma once

#include <include/nlohmann/json.hpp>
#include <stack>
#include <filesystem>
#include <iostream>
#include <fstream>

class ServerCommands {
private:
    std::filesystem::path rootPath = "server/tables";
    std::string logsPath;
    std::string logsEndline;
public:
    ServerCommands(std::string logsPath, std::string logsEndline) {
        this->logsPath = logsPath;
        this->logsEndline = logsEndline;
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
        else if (command == "get_table_data") {
            result = get_table_data(args);
        }
        else if (command == "change_table_entry") {
            result = change_table_entry(args);
        }
        else if (command == "check_json_integrity") {
            result = check_all_json_integrity();
        }
        else {
            log("The command \"" + command + "\" doesn't exist." + logsEndline);
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
            for (const auto& entry : std::filesystem::directory_iterator(currentDirectory)) {
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
        std::ifstream ifs(rootPath.string() + "/" + tableName);
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
        std::ofstream ofs(rootPath.string() + "/" + tableName);
        ofs << tableJson.dump(4);
        ofs.close();

        return tableJson;
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

    nlohmann::json check_all_json_integrity() {
        nlohmann::json result;

        nlohmann::json fileTree;
        std::stack<std::pair<std::filesystem::path, nlohmann::json*>> directories;
        directories.push({rootPath, &fileTree});

        while (!directories.empty()) {
            auto [currentDirectory, currentJson] = directories.top();
            directories.pop();
            // для всех файлов в папке
            for (const auto& entry : std::filesystem::directory_iterator(currentDirectory)) {
                std::string entryPath = entry.path().filename().string();
                // если это папка
                if (entry.is_directory()) {
                    directories.push({entry.path(), &(*currentJson)[entryPath]});
                }
                else {
                    // если это файл
                    try {
                        // проверка целостности, занесение результатов
                        nlohmann::json fileCheckResult;
                        fileCheckResult["file"] = entry.path();
                        if (check_json_integrity(entry.path())) {
                            fileCheckResult["status"] = "valid";
                        }
                        else {
                            fileCheckResult["status"] = "invalid";
                        }

                        result.push_back(fileCheckResult);
                    } catch (const std::exception& exception) {
                        log("JSON integrity check failed for file: " + entry.path().string() + ". Error: " + exception.what() + logsEndline);
                    }
                }
            }
        }

        return result;
    }

    bool check_json_integrity(std::string filePath) {
        std::ifstream ifs(filePath);
        nlohmann::json jsonFile;
        try {
            jsonFile = nlohmann::json::parse(ifs); 
        }
        catch (const std::exception& e) {
            log("JSON parsing failed for file: " + filePath + ". Error: " + e.what() + logsEndline);
            ifs.close();
            return false;
        }
        ifs.close();

        if (jsonFile.is_null()) {
            log("Integrity check failed for the file: " + filePath + ". JSON file is null." + logsEndline);
            return false;
        }
        if (jsonFile.empty()) {
            log("Integrity check failed for the file: " + filePath + ". JSON file is empty." + logsEndline);
            return false;
        }
        if (!jsonFile.is_array()) {
            log("Integrity check failed for the file: " + filePath + ". JSON file is not an array." + logsEndline);
            return false;
        }

        const nlohmann::json firstRow = jsonFile[0];
        if (!firstRow.is_object()) {
            log("Integrity check passed for the file: " + filePath + ". First row isn't an object." + logsEndline);
            return false;
        }
        // получение ключей первой строки
        std::vector<std::string> rowKeys;
        for (auto it = firstRow.begin(); it != firstRow.end(); it++) {
            rowKeys.push_back(it.key());
        }
        if (rowKeys.empty()) {
            log("Integrity check failed for the file: " + filePath + ". First row has no keys." + logsEndline);
            return false;
        }

        for (int i = 0; i < jsonFile.size(); ++i) {
            const auto row = jsonFile[i];
            
            if (!row.is_object()) {
                log("Integrity check failed for the file: " + filePath + ". Row " + std::to_string(i) + " is not an object." + logsEndline);
                return false;
            }
            // проверка количества ключей
            if (row.size() != rowKeys.size()) {
                log("Integrity check failed for the file: " + filePath + ". Row " + std::to_string(i) + " has a mismatched number of keys." + logsEndline);
                return false;
            }
            // проверка наличия каждого из ключей
            for (const auto key : rowKeys) {
                if (row.find(key) == row.end()) {
                    log("Integrity check failed for the file: " + filePath + ". Row " + std::to_string(i) + " is missing the key \"" + key + "\"." + logsEndline);
                    return false;
                }
            }
        }

        log("Integrity check passed for the file: " + filePath + "." + logsEndline);
        return true;
    }

    void log(const std::string message) {
        std::ofstream logFile(logsPath, std::ios_base::app);
        if (logFile.is_open()) {
            logFile << get_current_time() << ": " << message << "\n";
        }
    }
};