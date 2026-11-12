#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

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

    // Скачивает PW_RU_Voice.pak и PW_RU_Subs.pak и ставит их в ~mods/
    bool DownloadAndInstallAll() const;

    // Скачивает PW_RU_UE4SS.zip, распаковывает и ставит UE4SS + интро
    bool DownloadAndInstallUE4SS() const;

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

    bool ExtractZip(const std::filesystem::path& zipPath,
                    const std::filesystem::path& destDir) const;

    bool DeployUE4SSFiles(const std::filesystem::path& extractedRoot) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};