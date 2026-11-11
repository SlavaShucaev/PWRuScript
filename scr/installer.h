#pragma once

#include <filesystem>

class Installer {
public:
    Installer(
        const std::filesystem::path& contentDir,
        const std::filesystem::path& voicePak,
        const std::filesystem::path& subtitlePak
        );

    // Проверка уже установленных компонентов.
    // Возвращает true, если найден хотя бы один из них.
    // Печатает предупреждения и (при needPause) ждёт Enter.
    bool CheckExistingInstallation(bool needPause = true) const;

    void InstallVoice() const;
    void InstallSubtitles() const;

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
        ) const;

    // Проверяет один файл, при наличии выводит Warning.
    // Возвращает true, если файл существует.
    bool CheckFile(const std::filesystem::path& p) const;

    // Проверяет папку (Mods/).
    bool CheckDirectory(const std::filesystem::path& p) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};