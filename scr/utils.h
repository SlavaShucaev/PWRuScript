#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <dirent.h>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <random>

namespace utils {

/*=== ASCII-арт ===*/
inline void PrintSideBySide(const std::vector<std::string>& artLines,
                            const std::vector<std::string>& textLines,
                            std::size_t gap = 4) {
    // Ширина арта = максимальная длина строки.
    std::size_t artWidth = 0;
    for (const auto& line : artLines) {
        artWidth = std::max(artWidth, line.size());
    }

    const std::size_t totalLines =
        std::max(artLines.size(), textLines.size());

    for (std::size_t i = 0; i < totalLines; ++i) {
        const std::string& art  = i < artLines.size()  ? artLines[i]  : std::string{};
        const std::string& text = i < textLines.size() ? textLines[i] : std::string{};

        std::cout << art;
        if (art.size() < artWidth) {
            std::cout << std::string(artWidth - art.size(), ' ');
        }
        std::cout << std::string(gap, ' ') << text << '\n';
    }
}

/*===Вывод случайной цитаты===*/

template <typename T>
inline const T& PickRandom(const std::vector<T>& items) {
    static thread_local std::mt19937 rng(std::random_device{}());
    if (items.empty()) {
        static const T empty{};
        return empty;
    }
    std::uniform_int_distribution<std::size_t> dist(0, items.size() - 1);
    return items[dist(rng)];
}

/*===Проверка, запущен ли игра===*/

inline bool IsProcessRunning(const std::string& target_name) {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return false;
    }

    struct dirent* entry;
    bool found = false;

    while ((entry = readdir(proc_dir)) != nullptr) {
        bool is_number = true;
        for (char* p = entry->d_name; *p; ++p) {
            if (!std::isdigit(*p)) {
                is_number = false;
                break;
            }
        }

        if (!is_number) continue;

        std::string comm_path = std::string("/proc/") + entry->d_name + "/comm";
        std::ifstream comm_file(comm_path);
        if (comm_file.is_open()) {
            std::string proc_name;
            std::getline(comm_file, proc_name);
            if (proc_name == target_name.substr(0, 15)) {
                found = true;
                break;
            }
        }
    }

    closedir(proc_dir);
    return found;
}

/*===Вывод сообщений===*/

enum class Status {
    Success,
    Warning,
    Error,
    Info
};

inline void Print(const std::string& message) {
    std::cout << message << '\n';
}

inline void Print(const std::string& message, Status messageStatus) {
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

    (*out) << tag << ' ';

    std::string indent(tagLen + 1, ' ');
    for (std::size_t i = 0; i < message.size(); ++i) {
        if (message[i] == '\n') {
            (*out) << '\n' << indent;
        } else {
            (*out) << message[i];
        }
    }
    (*out) << '\n';
}

/*===Работа с файловой системой===*/

// Обёртка для единообразной обработки ошибок ФС.
inline bool CheckPath(const std::filesystem::path& path,
                      bool expectDirectory,
                      const std::string& description) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec) && !ec;
    const bool correctType = expectDirectory
        ? std::filesystem::is_directory(path, ec)
        : std::filesystem::is_regular_file(path, ec);

    if (!exists || ec || !correctType) return false;

    Print("Найден уже установленный " + description + ":\n" + path.string(),
          Status::Warning);
    return true;
}

// Удаляет файл, игнорируя ошибки (для временных файлов).
inline void RemoveQuietly(const std::filesystem::path& path) {
    std::error_code ignore;
    std::filesystem::remove(path, ignore);
}

/*===Подтверждение действия (Да/Нет)===*/

// UTF-8-aware приведение кириллицы к нижнему регистру.
inline std::string ToLowerUtf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);

        if (c < 0x80) {
            out.push_back(static_cast<char>(std::tolower(c)));
            ++i;
            continue;
        }

        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);

            if (c == 0xD0 && c2 >= 0x90 && c2 <= 0x9F) {
                out.push_back(static_cast<char>(0xD0));
                out.push_back(static_cast<char>(c2 + 0x20));
                i += 2;
                continue;
            }
            if (c == 0xD0 && c2 >= 0xA0 && c2 <= 0xAF) {
                out.push_back(static_cast<char>(0xD1));
                out.push_back(static_cast<char>(c2 - 0x20));
                i += 2;
                continue;
            }
            if (c == 0xD0 && c2 == 0x81) {
                out.push_back(static_cast<char>(0xD1));
                out.push_back(static_cast<char>(0x91));
                i += 2;
                continue;
            }

            out.push_back(static_cast<char>(c));
            out.push_back(static_cast<char>(c2));
            i += 2;
            continue;
        }

        out.push_back(static_cast<char>(c));
        ++i;
    }
    return out;
}

inline bool Confirm(const std::string& question) {
    std::cout << question << " [д/н]: " << std::flush;

    while (true) {
        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << '\n';
            return false;
        }

        auto start = input.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            std::cout << "Пожалуйста, введите «да» или «нет»: " << std::flush;
            continue;
        }
        auto end = input.find_last_not_of(" \t\r\n");
        std::string trimmed = input.substr(start, end - start + 1);

        std::string lower = ToLowerUtf8(trimmed);

        if (lower == "д" || lower == "да" ||
            lower == "y" || lower == "yes") {
            return true;
        }
        if (lower == "н" || lower == "нет" ||
            lower == "n" || lower == "no") {
            return false;
        }

        std::cout << "Пожалуйста, введите «да» или «нет»: " << std::flush;
    }
}

} // namespace utils