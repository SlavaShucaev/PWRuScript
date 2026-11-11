#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Locator {
public:
    Locator();

    // Возвращает путь к игре или пустой path, если не найдено.
    std::filesystem::path GetGamePath();

    // Позволяет пользователю вручную ввести путь, повторяет запрос
    // пока не получит валидную директорию (или пустую строку для отказа).
    std::filesystem::path AskUserForGamePath();

    static bool IsValidDirectory(const std::string& targetDir);
    static bool IsValidFile(const std::string& targetFile);

private:
    std::vector<std::filesystem::path> CandidateSteamPaths() const;
    std::vector<std::filesystem::path> ReadSteamLibraries(
        const std::filesystem::path& steamRoot) const;

    std::filesystem::path homeDir_;
};