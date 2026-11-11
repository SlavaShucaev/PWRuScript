#pragma once

#include <filesystem>
#include <string>

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

    // Скачивает PW_RU_Voice.pak и PW_RU_Subs.pak из GitHub Releases
    // и устанавливает их в Content/Paks/~mods/.
    // Возвращает false при любой ошибке (сеть, скачивание, установка).
    bool DownloadAndInstallAll() const;

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
        ) const;

    // Проверяет один файл, при наличии выводит Warning.
    bool CheckFile(const std::filesystem::path& p) const;

    // Проверяет папку (Mods/).
    bool CheckDirectory(const std::filesystem::path& p) const;

    // Скачивает url в destFile. Возвращает false при ошибке
    // (сеть, HTTP >= 400, пустой файл, ошибка записи).
    bool DownloadFile(const std::string& url,
                      const std::filesystem::path& destFile) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};