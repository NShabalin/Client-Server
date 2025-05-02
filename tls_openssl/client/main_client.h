#pragma once

#include <iostream>
#include <client/client.h>

bool check_json(const nlohmann::json file) {
    if (file.empty() || file.is_null()) {
        std::cout << "Ошибка при получении данных с сервера. Попробуйте еще раз.";
        return false;
    }
    return true;
}

int get_int_input(int from, int to) {
    int input;
    while (true) {
        std::cout << "Введите число от " << from << " до " << to << ":\n";
        std::cin >> input;

        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Некорректный ввод. Попробуйте снова.\n";
        }
        else {
            if (input < from || input > to) {
                std::cout << "Число вне допустимых значений. Попробуйте снова.\n";
            }
            else break;
        }
    }
    return input;
}

void get_table_data(Client client) {
    std::cout << "Список доступных таблиц:\n";
    nlohmann::json outputJson = client.send_command("get_all_tables").getData();
    if (!check_json(outputJson)) return;

    int count = 0;
    std::vector<std::string> tableNames;
    for (auto item : outputJson) {
        count++;
        std::cout << count << ": " << item << "\n";
        tableNames.push_back(item);
    }

    std::cout << "Введите номер таблицы, данные которой вы хотите получить:\n";
    int tableNumber = get_int_input(1, count);
    
    std::string tableName = tableNames[tableNumber - 1];
    std::vector<std::string> args; args.push_back(tableName);

    std::cout << "Данные таблицы " + tableName + ":\n";
    nlohmann::json tableData = client.send_command("get_table_data", args).getData();
    if (!check_json(tableData)) return;
    std::cout << std::setw(4) << tableData << "\n";
}

void change_table_entry(Client client) {
    std::cout << "Список доступных таблиц:\n";
    nlohmann::json outputJson = client.send_command("get_all_tables").getData();
    if (!check_json(outputJson)) return;
    int count = 0;
    std::vector<std::string> tableNames;
    for (auto item : outputJson) {
        count++;
        std::cout << count << ": " << item << "\n";
        tableNames.push_back(item);
    }
    std::cout << "Введите номер таблицы, данные которой вы хотите получить:\n";
    int tableNumber = get_int_input(1, count);
    
    std::string tableName = tableNames[tableNumber - 1];
    std::vector<std::string> args; args.push_back(tableName);

    std::cout << "Данные таблицы " + tableName + ":\n";
    nlohmann::json tableData = client.send_command("get_table_data", args).getData();
    if (!check_json(tableData)) return;
    count = 0;
    std::vector<nlohmann::json> rows;
    for (auto item : tableData) {
        count++;
        std::cout << count << ": " << item << "\n";
        rows.push_back(item);
    }

    std::cout << "Введите номер строки, которую вы хотите изменить.\n";
    int rowNum = get_int_input(1, count);
    nlohmann::json row = rows[rowNum - 1];

    count = 0;
    std::vector<std::string> cols;
    for (auto [key, value] : row.items()) {
        count++;
        std::cout << count << ": " << key << ": " << value << "\n";
        cols.push_back(key);
    }
    std::cout << "Введите номер столбца, который вы хотите изменить:\n";
    int colNumber = get_int_input(1, count);
    std::string colName = cols[colNumber - 1]; 
    
    std::string newValue;
    std::cout << "Введите новое значение клетки:\n";
    std::cin.ignore();
    std::getline(std::cin, newValue);

    args.push_back(std::to_string(rowNum - 1));
    args.push_back(colName);
    args.push_back(newValue);

    std::cout << "Данные таблицы после изменения клетки:\n";
    nlohmann::json tableDataChanged = client.send_command("change_table_entry", args).getData();
    if (!check_json(tableDataChanged)) return;
    std::cout << std::setw(4) << tableDataChanged << "\n";
}

void run_client() {
    Client client;

    while(true) {
        std::cout << "1 - получить список таблиц на сервере\n";
        std::cout << "2 - получить все данные одной из таблиц\n";
        std::cout << "3 - изменить клетку в таблице\n";
        std::cout << "4 - проверить целостность файлов\n";
        std::cout << "0 - выход\n\n";

        int input = get_int_input(0, 4);

        nlohmann::json outputJson;
        switch(input) {
            case 1: 
                std::cout << "Список всех таблиц:\n";
                outputJson = client.send_command("get_all_tables").getData();
                if (!check_json(outputJson)) break;
                std::cout << std::setw(4) << outputJson << "\n";
                break;
            case 2:
                get_table_data(client);
                break;
            case 3:
                change_table_entry(client);
                break;
            case 4:
                std::cout << "Целостность таблиц:\n";
                outputJson = client.send_command("check_json_integrity").getData();
                if (!check_json(outputJson)) break;
                std::cout << std::setw(4) << outputJson << "\n";
                break;
            case 0: 
                client.WSA_cleanup();
                exit(0);
        }

        std::cout << "\nНажмите любую клавишу для продолжения...\n";
        std::cin.ignore();
        std::cin.get();
    }
}


