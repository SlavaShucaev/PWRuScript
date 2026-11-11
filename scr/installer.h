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

    bool CheckExistingInstallation(bool needPause = true) const;
    void InstallVoice() const;
    void InstallSubtitles() const;

    // Скачивает PW_RU_Voice.pak и PW_RU_Subs.pak из GitHub Releases
    // и устанавливает их в Content/Paks/~mods/.
    bool DownloadAndInstallAll() const;

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
        ) const;

    bool CheckFile(const std::filesystem::path& p) const;
    bool CheckDirectory(const std::filesystem::path& p) const;

    // Скачивает url в destFile с прогресс-баром.
    // label — человекочитаемое имя («Озвучка», «Субтитры»).
    bool DownloadFile(const std::string& url,
                      const std::filesystem::path& destFile,
                      const std::string& label) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};