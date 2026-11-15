#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

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

    bool CheckExistingInstallation(bool needPause = true) const;

    void InstallVoice() const;
    void InstallSubtitles() const;

    bool DownloadAndInstallAll() const;
    bool DownloadAndInstallUE4SS() const;
    bool ApplyLanguageFix() const;

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
        ) const;

    // Скачивает pak из GitHub и кладёт в ~mods/ с перезаписью.
    bool DownloadPakToMods(const std::string& fileName,
                           const std::string& label) const;

    bool DownloadFile(const std::string& url,
                      const std::filesystem::path& destFile,
                      const std::string& label) const;

    bool ExtractZip(const std::filesystem::path& zipPath,
                    const std::filesystem::path& destDir) const;

    // Копирует файл с overwrite и логирует ошибку.
    bool CopyFileOrLog(const std::filesystem::path& src,
                       const std::filesystem::path& dst,
                       const std::string& description) const;

    bool DeployUE4SSFiles(const std::filesystem::path& extractedRoot) const;

    // Применяет фикс языка к буферу; возвращает true, если что-то заменено.
    static bool ReplaceLanguageInBuffer(std::vector<char>& buffer);

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};