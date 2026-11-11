#include "locator.h"
#include "utils.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace {
constexpr const char* kGameFolderName = "Project Wingman";
}

Locator::Locator() {
    if (const char* env = std::getenv("HOME")) {
        homeDir_ = env;
    } else if (const char* env = std::getenv("USERPROFILE")) {
        // на случай запуска под Wine/подобным окружением
        homeDir_ = env;
    }
}

// Все возможные корни Steam на Linux.
std::vector<fs::path> Locator::CandidateSteamPaths() const {
    std::vector<fs::path> result;
    if (homeDir_.empty()) return result;

    const fs::path home = homeDir_;

    // Классический симлинк
    result.push_back(home / ".steam" / "steam");
    // Flatpak
    result.push_back(home / ".var" / "app" / "com.valvesoftware.Steam"
                     / ".local" / "share" / "Steam");
    // Snap
    result.push_back(home / "snap" / "steam" / "common" / ".local"
                     / "share" / "Steam");
    // Нативный XDG-путь
    result.push_back(home / ".local" / "share" / "Steam");
    // Debian/Ubuntu иногда
    result.push_back(home / ".steam" / "root");

    // Убираем несуществующие
    std::vector<fs::path> filtered;
    for (auto& p : result) {
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_directory(p, ec)) {
            filtered.push_back(p);
        }
    }
    return filtered;
}

// Читает libraryfolders.vdf и вытаскивает все "path" "..."
std::vector<fs::path> Locator::ReadSteamLibraries(
    const fs::path& steamRoot) const {
    std::vector<fs::path> libs;
    const fs::path vdf = steamRoot / "steamapps" / "libraryfolders.vdf";

    std::ifstream in(vdf);
    if (!in) return libs;

    // Грубый, но устойчивый разбор: ищем строки вида:  "path"   "/some/path"
    static const std::regex re(R"re("path"\s+"([^"]+)")re");
    std::string line;
    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            std::string p = m[1].str();
            // разэкранирование \\ -> \ (VDF экранирует бэкслеши)
            std::string unescaped;
            unescaped.reserve(p.size());
            for (std::size_t i = 0; i < p.size(); ++i) {
                if (p[i] == '\\' && i + 1 < p.size() && p[i + 1] == '\\') {
                    unescaped.push_back('\\');
                    ++i;
                } else {
                    unescaped.push_back(p[i]);
                }
            }
            libs.emplace_back(unescaped);
        }
    }
    return libs;
}

fs::path Locator::GetGamePath() {
    // 1. Проходим по всем известным корням Steam.
    for (const auto& steamRoot : CandidateSteamPaths()) {
        // 1.1. Стандартная папка внутри этого корня.
        fs::path guess = steamRoot / "steamapps" / "common" / kGameFolderName;
        if (IsValidDirectory(guess.string())) {
            return guess;
        }

        // 1.2. Дополнительные библиотеки.
        for (const auto& lib : ReadSteamLibraries(steamRoot)) {
            fs::path p = lib / "steamapps" / "common" / kGameFolderName;
            if (IsValidDirectory(p.string())) {
                return p;
            }
        }
    }
    return {};
}

// Запрашивает путь у пользователя.
fs::path Locator::AskUserForGamePath() {
    utils::Print(
        "Не удалось автоматически найти папку Project Wingman.\n"
        "Введите путь к папке игры (например: "
        "/home/user/.steam/steam/steamapps/common/Project Wingman).\n"
        "Пустая строка — отмена.",
        utils::Status::Info);

    while (true) {
        std::cout << "> " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return {};
        }
        // Тримминг пробелов по краям
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return {};
        auto end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);

        // Убираем возможные кавычки, если пользователь их поставил
        if (trimmed.size() >= 2 &&
            ((trimmed.front() == '"' && trimmed.back() == '"') ||
             (trimmed.front() == '\'' && trimmed.back() == '\''))) {
            trimmed = trimmed.substr(1, trimmed.size() - 2);
        }

        // Принимаем как папку самой игры, так и её родителя —
        // если пользователь указал .../common, то добавляем имя игры.
        fs::path p(trimmed);
        if (IsValidDirectory(p.string())) {
            fs::path candidate = p / kGameFolderName;
            if (IsValidDirectory(candidate.string())) {
                return candidate;     // был указан .../common
            }
            // Проверим, похоже ли это на папку игры (наличие Paks)
            if (IsValidDirectory((p / "Paks").string())) {
                return p;
            }
            utils::Print(
                "В указанной папке не найдено содержимого игры "
                "(нет подпапки \"Paks\" или \"" + std::string(kGameFolderName) + "\").",
                utils::Status::Warning);
            continue;
        }

        utils::Print("Такой папки не существует: " + trimmed,
                     utils::Status::Error);
    }
}

bool Locator::IsValidDirectory(const std::string& targetDir) {
    std::error_code ec;
    return fs::exists(targetDir, ec) &&
           fs::is_directory(targetDir, ec);
}

bool Locator::IsValidFile(const std::string& targetFile) {
    std::error_code ec;
    return fs::exists(targetFile, ec) &&
           fs::is_regular_file(targetFile, ec);
}