#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <dirent.h>
#include <cstring>
#include <cctype>


namespace utils {

/*===Проверка, запущен ли игра===*/

inline bool IsProcessRunning(const std::string& target_name) {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return false;
    }

    struct dirent* entry;
    bool found = false;

    while ((entry = readdir(proc_dir)) != nullptr) {
        // Проверяем, состоит ли имя папки только из цифр (это PID)
        bool is_number = true;
        for (char* p = entry->d_name; *p; ++p) {
            if (!std::isdigit(*p)) {
                is_number = false;
                break;
            }
        }

        if (!is_number) continue;

        // Путь к файлу comm внутри /proc/<pid>/comm
        std::string comm_path = std::string("/proc/") + entry->d_name + "/comm";
        std::ifstream comm_file(comm_path);
        if (comm_file.is_open()) {
            std::string proc_name;
            std::getline(comm_file, proc_name);

            // comm обрезается ядром до 15 символов
            // Для проверки учитываем этот момент
            if (proc_name == target_name.substr(0,15)) {
                found = true;
                break;
            }
        }
    }

    closedir(proc_dir);
    return found;
}

/*===Вывод сообщений===*/

//Перечисление статусов
enum class Status{
    Success,
    Warning,
    Error,
    Info
};

// Выводит текст  одну строку
inline void Print(const std::string& message) {
    std::cout << message << '\n';
}

// Выводит текст в одну строку со статусом сообщения
// С возможностью мнгострочного вывода сообщения
inline void Print(const std::string& message, Status messageStatus){
    // Выбираем поток, тег и «чистую» длину тега (без ANSI-кодов)
    std::ostream* out = &std::cout;
    const char* tag   = nullptr;
    std::size_t tagLen = 0;

    switch (messageStatus) {
    case Status::Success: tag = "\033[1;32m[ ОК ]\033[0m";       tagLen = 6;  break;
    case Status::Error:   tag = "\033[1;31m[ ОШИБКА ]\033[0m";   tagLen = 10; out = &std::cerr; break;
    case Status::Warning: tag = "\033[1;33m[ ВНИМАНИЕ ]\033[0m"; tagLen = 12; break;
    case Status::Info:
    default:              tag = "\033[1;34m[ ИНФО ]\033[0m";     tagLen = 8;  break;
    }

    // Первая строка: тег + пробел + первая строка сообщения
    (*out) << tag << ' ';

    // Печатаем сообщение, подменяя '\n' на '\n' + отступ шириной tagLen+1
    std::string indent(tagLen + 1, ' ');   // +1 — пробел после тега
    for (std::size_t i = 0; i < message.size(); ++i) {
        if (message[i] == '\n') {
            (*out) << '\n' << indent;
        } else {
            (*out) << message[i];
        }
    }
    (*out) << '\n';
}

}