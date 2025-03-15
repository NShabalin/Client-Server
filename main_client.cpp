#include <iostream>
#include <client/client.h>

Client client;

void get_table_data() {
    std::cout << "Список доступных таблиц:\n";
    std::cout << std::setw(4) << client.send_command("get_all_tables").getData() << "\n";
    std::cout << "Введите название таблицы, данные которой вы хотите получить:\n";
    std::string tableName;
    std::cin >> tableName; // change it to choosing with a number from a list of tables
    std::vector<std::string> args; args.push_back(tableName);

    std::cout << "Данные таблицы " + tableName + ":\n";
    std::cout << std::setw(4) << client.send_command("get_table_data", args).getData() << "\n";
}

void change_table_entry() {
    std::cout << "Список доступных таблиц:\n";
    std::cout << std::setw(4) << client.send_command("get_all_tables").getData() << "\n";
    std::cout << "Введите название таблицы, данные которой вы хотите изменить:\n";
    std::string tableName;
    std::cin >> tableName; // change it to choosing with a number from a list of tables
    std::vector<std::string> args; args.push_back(tableName);

    std::cout << "Данные таблицы " + tableName + ":\n";
    std::cout << std::setw(4) << client.send_command("get_table_data", args).getData() << "\n";

    int rowNum = -1;
    std::cout << "Введите номер строки, которую вы хотите изменить.\n";
    std::cin >> rowNum; // add checkers

    std::string colName; 
    std::cout << "Введите название столбца, который вы хотите изменить:\n";
    std::cin >> colName;

    std::string newValue;
    std::cout << "Введите новое значение клетки:\n";
    std::cin >> newValue;

    args.push_back(std::to_string(rowNum - 1));
    args.push_back(colName);
    args.push_back(newValue);

    std::cout << "Данные таблицы после изменения клетки:\n";
    std::cout << std::setw(4) << client.send_command("change_table_entry", args).getData() << "\n";
}

int main() {
    while(true) {
        std::cout << "1 - получить список таблиц на сервере\n";
        std::cout << "2 - получить все данные одной из таблиц\n";
        std::cout << "3 - изменить клетку в таблице\n";
        std::cout << "0 - выход\n\n";

        int input = -1;
        std::cin >> input;

        switch(input) {
            case 1: 
                std::cout << "Список всех таблиц:\n";
                std::cout << std::setw(4) << client.send_command("get_all_tables").getData() << "\n";
                break;
            case 2:
                get_table_data();
                break;
            case 3:
                change_table_entry();
                break;
            case 0: 
                exit(0);
        }

        std::cout << "\nНажмите любую клавишу для продолжения...\n";
        std::cin.ignore();
        std::cin.get();
    }

    return 0;
}

