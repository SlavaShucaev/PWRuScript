#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

// Бросается, когда пользователь отказался продолжать установку.
class UserCancelled : public std::runtime_error {
public:
    explicit UserCancelled(const std::string& what)
        : std::runtime_error(what) {}
};

class Installer {
public:
    Installer(
        const std::filesystem::path& contentDir,
        const std::filesystem::path& voicePak,
        const std::filesystem::path& subtitlePak
        );

    // Проверяет уже установленные компоненты.
    // Если что-то найдено — предупреждает и (при needPause)
    // спрашивает разрешение на перезапись.
    // Бросает UserCancelled, если пользователь отказался.
    bool CheckExistingInstallation(bool needPause = true) const;

    void InstallVoice() const;
    void InstallSubtitles() const;

    bool DownloadAndInstallAll() const;

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
        ) const;

    bool CheckFile(const std::filesystem::path& p) const;
    bool CheckDirectory(const std::filesystem::path& p) const;

    bool DownloadFile(const std::string& url,
                      const std::filesystem::path& destFile,
                      const std::string& label) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};